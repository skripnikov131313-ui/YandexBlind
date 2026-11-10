#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "skai_ai.h"
#include "include/cef_task.h"
#include "include/cef_parser.h"
#include "include/cef_values.h"

#include <winhttp.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <knownfolders.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <chrono>
#include <utility>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ws2_32.lib")

namespace {
constexpr wchar_t kModelUrlHost[] = L"huggingface.co";
constexpr wchar_t kModelUrlPath[] =
    L"/gguf/Qwen3.5-2B-GGUF/resolve/main/qwen3.5-2b-Q4_K_M.gguf";
constexpr wchar_t kModelFileName[] = L"qwen3.5-2b-Q4_K_M.gguf";
constexpr char kModelSha256[] =
    "9a3d0aa6c920f096441139b6a254542da855223fdad06855ea00c6c087fd713d";
constexpr std::uint64_t kModelExpectedBytes = 1406600000ULL;
constexpr std::uint64_t kModelMaximumBytes = 1500000000ULL;
constexpr std::size_t kMaxRequestBytes = 256 * 1024;
constexpr std::size_t kMaxResponseBytes = 2 * 1024 * 1024;
constexpr int kServerReadyTimeoutMs = 90000;

std::atomic<bool> g_shutting_down{false};
std::mutex g_worker_mutex;
std::condition_variable g_worker_condition;
unsigned int g_active_workers = 0;

std::mutex g_model_mutex;
bool g_model_download_active = false;
bool g_model_verified_this_run = false;

std::mutex g_server_mutex;
HANDLE g_server_process = nullptr;
HANDLE g_server_job = nullptr;
unsigned short g_server_port = 0;
std::wstring g_server_api_key;

class UiNotifyTask final : public CefTask {
 public:
  UiNotifyTask(CefRefPtr<CefBrowser> browser, std::wstring script)
      : browser_(browser), script_(std::move(script)) {}

  void Execute() override {
    if (!browser_) return;
    const auto frame = browser_->GetMainFrame();
    if (frame) frame->ExecuteJavaScript(script_, frame->GetURL(), 0);
  }

 private:
  CefRefPtr<CefBrowser> browser_;
  std::wstring script_;
  IMPLEMENT_REFCOUNTING(UiNotifyTask);
};

std::wstring JsonQuote(const std::wstring& value) {
  std::wstring result;
  result.reserve(value.size() + 2);
  result.push_back(L'"');
  constexpr wchar_t hex[] = L"0123456789abcdef";
  for (wchar_t ch : value) {
    switch (ch) {
      case L'"': result += L"\\\""; break;
      case L'\\': result += L"\\\\"; break;
      case L'\b': result += L"\\b"; break;
      case L'\f': result += L"\\f"; break;
      case L'\n': result += L"\\n"; break;
      case L'\r': result += L"\\r"; break;
      case L'\t': result += L"\\t"; break;
      default:
        if (ch < 0x20) {
          result += L"\\u00";
          result.push_back(hex[(ch >> 4) & 0x0f]);
          result.push_back(hex[ch & 0x0f]);
        } else {
          result.push_back(ch);
        }
    }
  }
  result.push_back(L'"');
  return result;
}

void Notify(CefRefPtr<CefBrowser> browser, const std::wstring& method,
            const std::wstring& arguments) {
  if (!browser || g_shutting_down.load()) return;
  std::wstring script = L"window.skaiNative&&window.skaiNative." + method +
                        L"&&window.skaiNative." + method + L"(" +
                        arguments + L");";
  CefPostTask(TID_UI, new UiNotifyTask(browser, std::move(script)));
}

void NotifyStatus(CefRefPtr<CefBrowser> browser,
                  const std::wstring& status,
                  int percent,
                  const std::wstring& detail = L"") {
  Notify(browser, L"localAiStatus",
         JsonQuote(status) + L"," + std::to_wstring(percent) + L"," +
             JsonQuote(detail));
}

template <typename Fn>
bool LaunchWorker(Fn&& function) {
  {
    std::lock_guard<std::mutex> lock(g_worker_mutex);
    if (g_shutting_down.load()) return false;
    ++g_active_workers;
  }
  try {
    std::thread worker([task = std::forward<Fn>(function)]() mutable {
      struct Completion {
        ~Completion() {
          std::lock_guard<std::mutex> lock(g_worker_mutex);
          if (g_active_workers > 0) --g_active_workers;
          g_worker_condition.notify_all();
        }
      } completion;
      try {
        task();
      } catch (...) {
        // Worker failures are reported at their operation boundary.
      }
    });
    try {
      worker.detach();
    } catch (...) {
      if (worker.joinable()) worker.join();
      throw;
    }
    return true;
  } catch (...) {
    std::lock_guard<std::mutex> lock(g_worker_mutex);
    if (g_active_workers > 0) --g_active_workers;
    g_worker_condition.notify_all();
    return false;
  }
}

std::filesystem::path GetModelDirectory() {
  PWSTR local = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE,
                                 nullptr, &local)) || !local) {
    return {};
  }
  const auto directory = std::filesystem::path(local) / L"SkAi" / L"Models";
  CoTaskMemFree(local);
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  return error ? std::filesystem::path{} : directory;
}

std::filesystem::path GetModelPath() {
  const auto directory = GetModelDirectory();
  return directory.empty() ? std::filesystem::path{}
                           : directory / kModelFileName;
}

std::filesystem::path GetRuntimeDirectory() {
  wchar_t executable[MAX_PATH]{};
  const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return {};
  return std::filesystem::path(executable).parent_path() /
         L"runtime" / L"llama";
}

std::string ToLowerHex(const std::vector<UCHAR>& bytes) {
  constexpr char hex[] = "0123456789abcdef";
  std::string output;
  output.reserve(bytes.size() * 2);
  for (UCHAR value : bytes) {
    output.push_back(hex[(value >> 4) & 0x0f]);
    output.push_back(hex[value & 0x0f]);
  }
  return output;
}

bool HashFileSha256(const std::filesystem::path& path, std::string& digest,
                    const std::function<void(std::uint64_t)>& progress = {}) {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  std::vector<UCHAR> hash_object;
  std::vector<UCHAR> hash_bytes(32);
  HANDLE file = INVALID_HANDLE_VALUE;
  bool success = false;

  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                  nullptr, 0) < 0) {
    return false;
  }

  DWORD object_size = 0;
  DWORD result_size = 0;
  if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                        reinterpret_cast<PUCHAR>(&object_size),
                        sizeof(object_size), &result_size, 0) < 0) {
    goto cleanup;
  }
  hash_object.resize(object_size);
  if (BCryptCreateHash(algorithm, &hash, hash_object.data(), object_size,
                       nullptr, 0, 0) < 0) {
    goto cleanup;
  }

  file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                     nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
                     nullptr);
  if (file == INVALID_HANDLE_VALUE) goto cleanup;

  {
    std::array<UCHAR, 1024 * 1024> buffer{};
    std::uint64_t total = 0;
    DWORD read = 0;
    while (true) {
      if (!ReadFile(file, buffer.data(),
                    static_cast<DWORD>(buffer.size()), &read, nullptr)) {
        goto cleanup;
      }
      if (read == 0) break;
      if (BCryptHashData(hash, buffer.data(), read, 0) < 0) goto cleanup;
      total += read;
      if (progress) progress(total);
      if (g_shutting_down.load()) goto cleanup;
    }
  }

  if (BCryptFinishHash(hash, hash_bytes.data(),
                       static_cast<ULONG>(hash_bytes.size()), 0) < 0) {
    goto cleanup;
  }
  digest = ToLowerHex(hash_bytes);
  success = true;

cleanup:
  if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
  if (hash) BCryptDestroyHash(hash);
  if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
  return success;
}

bool VerifyModel(const std::filesystem::path& path) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error) || error) return false;
  const auto bytes = std::filesystem::file_size(path, error);
  if (error || bytes < 1200000000ULL) return false;
  std::string digest;
  return HashFileSha256(path, digest) && digest == kModelSha256;
}

bool ReadHttpStatus(HINTERNET request, DWORD& status) {
  DWORD size = sizeof(status);
  return WinHttpQueryHeaders(request,
      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
      WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
      WINHTTP_NO_HEADER_INDEX) != FALSE;
}

bool DownloadModel(CefRefPtr<CefBrowser> browser) {
  const auto model_path = GetModelPath();
  if (model_path.empty()) {
    NotifyStatus(browser, L"error", 0,
                 L"Не удалось создать каталог модели в LocalAppData.");
    return false;
  }
  auto temporary_path = model_path;
  temporary_path += L".part";

  HINTERNET session = WinHttpOpen(
      L"SkAi Local Model Installer/1.0",
      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) {
    NotifyStatus(browser, L"error", 0, L"Не удалось открыть HTTPS-соединение.");
    return false;
  }
  // Short receive slices make shutdown responsive without imposing a
  // minimum sustained download speed. A stalled connection is retried below.
  WinHttpSetTimeouts(session, 10000, 5000, 10000, 1000);

  HINTERNET connection = WinHttpConnect(
      session, kModelUrlHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
  HINTERNET request = connection
      ? WinHttpOpenRequest(connection, L"GET", kModelUrlPath, nullptr,
                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                           WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH)
      : nullptr;

  bool success = false;
  HANDLE output = INVALID_HANDLE_VALUE;
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  std::vector<UCHAR> hash_object;
  std::vector<UCHAR> hash_bytes(32);

  if (!request || !WinHttpSendRequest(request,
          WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
      !WinHttpReceiveResponse(request, nullptr)) {
    NotifyStatus(browser, L"error", 0,
                 L"Не удалось скачать модель с Hugging Face.");
    goto cleanup;
  }

  {
    DWORD status = 0;
    if (!ReadHttpStatus(request, status) || status != 200) {
      NotifyStatus(browser, L"error", 0,
                   L"Сервер модели вернул HTTP " + std::to_wstring(status) + L".");
      goto cleanup;
    }
  }

  output = CreateFileW(temporary_path.c_str(), GENERIC_WRITE, 0, nullptr,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL |
                       FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (output == INVALID_HANDLE_VALUE) {
    NotifyStatus(browser, L"error", 0,
                 L"Не удалось создать временный файл модели.");
    goto cleanup;
  }

  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                  nullptr, 0) < 0) {
    NotifyStatus(browser, L"error", 0,
                 L"Не удалось инициализировать проверку SHA-256.");
    goto cleanup;
  }

  {
    DWORD object_size = 0;
    DWORD returned = 0;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                          reinterpret_cast<PUCHAR>(&object_size),
                          sizeof(object_size), &returned, 0) < 0) {
      NotifyStatus(browser, L"error", 0,
                   L"Не удалось инициализировать SHA-256.");
      goto cleanup;
    }
    hash_object.resize(object_size);
    if (BCryptCreateHash(algorithm, &hash, hash_object.data(), object_size,
                         nullptr, 0, 0) < 0) {
      NotifyStatus(browser, L"error", 0,
                   L"Не удалось создать SHA-256 hash.");
      goto cleanup;
    }
  }

  {
    std::array<UCHAR, 1024 * 1024> buffer{};
    std::uint64_t total = 0;
    DWORD bytes_read = 0;
    int last_percent = -1;
    ULONGLONG last_progress_at = GetTickCount64();
    do {
      if (g_shutting_down.load()) {
        NotifyStatus(browser, L"cancelled", 0, L"Загрузка остановлена.");
        goto cleanup;
      }
      bytes_read = 0;
      if (!WinHttpReadData(request, buffer.data(),
                           static_cast<DWORD>(buffer.size()), &bytes_read)) {
        const DWORD read_error = GetLastError();
        if (read_error == ERROR_WINHTTP_TIMEOUT &&
            !g_shutting_down.load() &&
            GetTickCount64() - last_progress_at < 60000) {
          continue;
        }
        if (g_shutting_down.load()) {
          NotifyStatus(browser, L"cancelled", 0, L"Загрузка остановлена.");
        } else {
          NotifyStatus(browser, L"error", 0,
                       L"Загрузка остановлена: нет данных от сервера в течение минуты.");
        }
        goto cleanup;
      }
      if (bytes_read == 0) break;
      last_progress_at = GetTickCount64();
      if (total + bytes_read > kModelMaximumBytes) {
        NotifyStatus(browser, L"error", 0,
                     L"Размер модели превышает ожидаемый безопасный предел.");
        goto cleanup;
      }

      DWORD written = 0;
      if (!WriteFile(output, buffer.data(), bytes_read, &written, nullptr) ||
          written != bytes_read ||
          BCryptHashData(hash, buffer.data(), bytes_read, 0) < 0) {
        NotifyStatus(browser, L"error", 0,
                     L"Не удалось сохранить или проверить файл модели.");
        goto cleanup;
      }
      total += bytes_read;
      const int percent = static_cast<int>(
          std::min<std::uint64_t>(100, total * 100 / kModelExpectedBytes));
      if (percent != last_percent) {
        last_percent = percent;
        NotifyStatus(browser, L"downloading", percent,
                     L"Загружено " + std::to_wstring(total / (1024 * 1024)) +
                         L" МБ из примерно 1342 МБ");
      }
    } while (bytes_read > 0);

    if (total < 1200000000ULL ||
        BCryptFinishHash(hash, hash_bytes.data(),
                         static_cast<ULONG>(hash_bytes.size()), 0) < 0) {
      NotifyStatus(browser, L"error", 0,
                   L"Файл модели получен не полностью.");
      goto cleanup;
    }
  }

  if (ToLowerHex(hash_bytes) != kModelSha256) {
    NotifyStatus(browser, L"error", 0,
                 L"SHA-256 модели не совпал. Файл удалён.");
    goto cleanup;
  }

  if (!FlushFileBuffers(output)) {
    NotifyStatus(browser, L"error", 0,
                 L"Не удалось завершить запись модели на диск.");
    goto cleanup;
  }
  CloseHandle(output);
  output = INVALID_HANDLE_VALUE;
  if (!MoveFileExW(temporary_path.c_str(), model_path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    NotifyStatus(browser, L"error", 0,
                 L"Не удалось установить проверенную модель.");
    goto cleanup;
  }

  {
    std::lock_guard<std::mutex> lock(g_model_mutex);
    g_model_verified_this_run = true;
  }
  success = true;
  NotifyStatus(browser, L"ready", 100,
               L"Qwen 3.5 2B установлена и проверена.");

cleanup:
  if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
  if (hash) BCryptDestroyHash(hash);
  if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
  if (!success) {
    std::error_code error;
    std::filesystem::remove(temporary_path, error);
  }
  if (request) WinHttpCloseHandle(request);
  if (connection) WinHttpCloseHandle(connection);
  WinHttpCloseHandle(session);
  return success;
}

std::wstring MakeApiKey() {
  std::array<UCHAR, 32> bytes{};
  if (BCryptGenRandom(nullptr, bytes.data(),
                      static_cast<ULONG>(bytes.size()),
                      BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
    return {};
  }
  constexpr wchar_t hex[] = L"0123456789abcdef";
  std::wstring key;
  key.reserve(bytes.size() * 2);
  for (UCHAR value : bytes) {
    key.push_back(hex[(value >> 4) & 0x0f]);
    key.push_back(hex[value & 0x0f]);
  }
  return key;
}

bool ReserveLoopbackPort(unsigned short& port) {
  WSADATA data{};
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
  SOCKET socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_handle == INVALID_SOCKET) {
    WSACleanup();
    return false;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  bool success = false;
  if (bind(socket_handle, reinterpret_cast<sockaddr*>(&address),
           sizeof(address)) == 0) {
    int length = sizeof(address);
    if (getsockname(socket_handle, reinterpret_cast<sockaddr*>(&address),
                    &length) == 0) {
      port = ntohs(address.sin_port);
      success = port != 0;
    }
  }
  closesocket(socket_handle);
  WSACleanup();
  return success;
}

bool ProcessIsRunning(HANDLE process) {
  if (!process) return false;
  DWORD exit_code = 0;
  return GetExitCodeProcess(process, &exit_code) && exit_code == STILL_ACTIVE;
}

bool HttpGetHealth(unsigned short port) {
  HINTERNET session = WinHttpOpen(
      L"SkAi Local Runtime/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) return false;
  WinHttpSetTimeouts(session, 1000, 1000, 1000, 1500);
  HINTERNET connection = WinHttpConnect(
      session, L"127.0.0.1", port, 0);
  HINTERNET request = connection
      ? WinHttpOpenRequest(connection, L"GET", L"/health", nullptr,
                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                           0)
      : nullptr;
  bool ready = false;
  if (request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS,
                                    0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
      WinHttpReceiveResponse(request, nullptr)) {
    DWORD status = 0;
    ready = ReadHttpStatus(request, status) && status == 200;
  }
  if (request) WinHttpCloseHandle(request);
  if (connection) WinHttpCloseHandle(connection);
  WinHttpCloseHandle(session);
  return ready;
}

bool EnsureServer(unsigned short& port, std::wstring& api_key,
                  std::wstring& error_message) {
  std::lock_guard<std::mutex> lock(g_server_mutex);
  if (g_shutting_down.load()) {
    error_message = L"Приложение закрывается.";
    return false;
  }

  if (ProcessIsRunning(g_server_process) && g_server_port != 0 &&
      !g_server_api_key.empty()) {
    port = g_server_port;
    api_key = g_server_api_key;
    return true;
  }
  if (g_server_process) {
    CloseHandle(g_server_process);
    g_server_process = nullptr;
    g_server_port = 0;
    g_server_api_key.clear();
  }
  if (g_server_job) {
    CloseHandle(g_server_job);
    g_server_job = nullptr;
  }

  const auto model_path = GetModelPath();
  bool model_verified = false;
  {
    std::lock_guard<std::mutex> model_lock(g_model_mutex);
    model_verified = g_model_verified_this_run;
  }
  if (!model_verified) {
    if (!VerifyModel(model_path)) {
      error_message =
          L"Локальная модель не найдена или не прошла проверку SHA-256. Установите её в карточке модели.";
      return false;
    }
    std::lock_guard<std::mutex> model_lock(g_model_mutex);
    g_model_verified_this_run = true;
  }

  const auto runtime_directory = GetRuntimeDirectory();
  const auto server_executable = runtime_directory / L"llama-server.exe";
  std::error_code fs_error;
  if (!std::filesystem::is_regular_file(server_executable, fs_error) ||
      fs_error) {
    error_message =
        L"Локальный runtime llama-server отсутствует в пакете Sk/Ai.";
    return false;
  }

  if (!ReserveLoopbackPort(port)) {
    error_message = L"Не удалось выбрать локальный порт для ИИ.";
    return false;
  }
  api_key = MakeApiKey();
  if (api_key.empty()) {
    error_message = L"Не удалось создать ключ локального runtime.";
    return false;
  }

  SYSTEM_INFO system_info{};
  GetSystemInfo(&system_info);
  const unsigned int processor_count =
      std::max(2u, system_info.dwNumberOfProcessors);
  const unsigned int threads = std::min(6u, processor_count > 1
                                               ? processor_count - 1
                                               : 1u);

  std::wstring command =
      L"\"" + server_executable.wstring() + L"\""
      L" --model \"" + model_path.wstring() + L"\""
      L" --host 127.0.0.1 --port " + std::to_wstring(port) +
      L" --alias Qwen3.5-2B --ctx-size 4096 --threads " + std::to_wstring(threads) +
      L" --threads-batch " + std::to_wstring(threads) +
      L" --parallel 1 --n-gpu-layers 0 --no-webui --api-key \"" +
      api_key + L"\"";

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESHOWWINDOW;
  startup.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION process{};
  std::vector<wchar_t> mutable_command(command.begin(), command.end());
  mutable_command.push_back(L'\0');

  if (!CreateProcessW(server_executable.c_str(), mutable_command.data(),
                      nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                      nullptr, runtime_directory.c_str(), &startup, &process)) {
    error_message = L"Не удалось запустить llama-server.exe. Код Windows: " +
                    std::to_wstring(GetLastError());
    return false;
  }
  CloseHandle(process.hThread);

  HANDLE job = CreateJobObjectW(nullptr, nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits{};
  job_limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  const bool job_ready = job &&
      SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                              &job_limits, sizeof(job_limits)) &&
      AssignProcessToJobObject(job, process.hProcess);
  if (!job_ready) {
    const DWORD job_error = GetLastError();
    TerminateProcess(process.hProcess, 1);
    WaitForSingleObject(process.hProcess, 3000);
    CloseHandle(process.hProcess);
    if (job) CloseHandle(job);
    error_message = L"Не удалось включить контроль жизненного цикла локального runtime. Код Windows: " +
                    std::to_wstring(job_error);
    return false;
  }

  g_server_process = process.hProcess;
  g_server_job = job;
  g_server_port = port;
  g_server_api_key = api_key;

  const ULONGLONG deadline = GetTickCount64() + kServerReadyTimeoutMs;
  while (GetTickCount64() < deadline && !g_shutting_down.load()) {
    if (!ProcessIsRunning(g_server_process)) {
      error_message =
          L"llama-server завершился при запуске. Проверьте комплект runtime.";
      CloseHandle(g_server_process);
      g_server_process = nullptr;
      if (g_server_job) {
        CloseHandle(g_server_job);
        g_server_job = nullptr;
      }
      g_server_port = 0;
      g_server_api_key.clear();
      return false;
    }
    if (HttpGetHealth(port)) return true;
    Sleep(250);
  }

  error_message = L"Локальная модель не успела загрузиться в память.";
  TerminateProcess(g_server_process, 1);
  WaitForSingleObject(g_server_process, 3000);
  CloseHandle(g_server_process);
  g_server_process = nullptr;
  if (g_server_job) {
    CloseHandle(g_server_job);
    g_server_job = nullptr;
  }
  g_server_port = 0;
  g_server_api_key.clear();
  return false;
}

std::string ToUtf8(const std::wstring& value) {
  if (value.empty()) return {};
  const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
      value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (size <= 0) return {};
  std::string output(static_cast<std::size_t>(size), '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
          static_cast<int>(value.size()), output.data(), size,
          nullptr, nullptr) != size) return {};
  return output;
}

bool PostChatRequest(unsigned short port, const std::wstring& api_key,
                     const std::wstring& body, std::wstring& answer,
                     std::wstring& error_message) {
  const std::string body_utf8 = ToUtf8(body);
  if (body_utf8.empty() || body_utf8.size() > kMaxRequestBytes) {
    error_message = L"Слишком большой или некорректный запрос.";
    return false;
  }

  HINTERNET session = WinHttpOpen(
      L"SkAi Local Assistant/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) {
    error_message = L"Не удалось подключиться к локальной модели.";
    return false;
  }
  WinHttpSetTimeouts(session, 5000, 5000, 30000, 180000);

  HINTERNET connection = WinHttpConnect(session, L"127.0.0.1", port, 0);
  HINTERNET request = connection
      ? WinHttpOpenRequest(connection, L"POST", L"/v1/chat/completions",
                           nullptr, WINHTTP_NO_REFERER,
                           WINHTTP_DEFAULT_ACCEPT_TYPES, 0)
      : nullptr;

  bool success = false;
  if (!request) {
    error_message = L"Не удалось открыть локальный API.";
    goto cleanup;
  }

  {
    const std::wstring headers =
        L"Content-Type: application/json\r\nAuthorization: Bearer " +
        api_key + L"\r\n";
    if (!WinHttpSendRequest(
            request, headers.c_str(), static_cast<DWORD>(headers.size()),
            const_cast<char*>(body_utf8.data()),
            static_cast<DWORD>(body_utf8.size()),
            static_cast<DWORD>(body_utf8.size()), 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
      error_message = L"Локальная модель не ответила на запрос.";
      goto cleanup;
    }
  }

  {
    DWORD status = 0;
    if (!ReadHttpStatus(request, status)) {
      error_message = L"Не удалось прочитать ответ локальной модели.";
      goto cleanup;
    }
    std::string response;
    std::array<char, 64 * 1024> buffer{};
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
      if (response.size() + available > kMaxResponseBytes) {
        error_message = L"Ответ локальной модели превышает допустимый размер.";
        goto cleanup;
      }
      DWORD read = 0;
      const DWORD chunk = std::min<DWORD>(
          available, static_cast<DWORD>(buffer.size()));
      if (!WinHttpReadData(request, buffer.data(), chunk, &read) || read == 0) {
        error_message = L"Ответ локальной модели оборвался.";
        goto cleanup;
      }
      response.append(buffer.data(), read);
      available = 0;
    }

    if (status < 200 || status >= 300) {
      error_message = L"Ошибка локальной модели (HTTP " +
                      std::to_wstring(status) + L").";
      goto cleanup;
    }

    const auto value = CefParseJSON(response.data(), response.size(),
                                    JSON_PARSER_RFC);
    if (!value || value->GetType() != VTYPE_DICTIONARY) {
      error_message = L"Локальная модель вернула некорректный JSON.";
      goto cleanup;
    }
    const auto root = value->GetDictionary();
    const auto choices = root ? root->GetList("choices") : nullptr;
    if (!choices || choices->GetSize() == 0 ||
        choices->GetType(0) != VTYPE_DICTIONARY) {
      error_message = L"В ответе локальной модели нет текста.";
      goto cleanup;
    }
    const auto choice = choices->GetDictionary(0);
    const auto message = choice ? choice->GetDictionary("message") : nullptr;
    if (!message || message->GetType("content") != VTYPE_STRING) {
      error_message = L"Локальная модель не вернула содержимое сообщения.";
      goto cleanup;
    }
    answer = message->GetString("content").ToWString();
    success = !answer.empty();
    if (!success) error_message = L"Локальная модель вернула пустой ответ.";
  }

cleanup:
  if (request) WinHttpCloseHandle(request);
  if (connection) WinHttpCloseHandle(connection);
  WinHttpCloseHandle(session);
  return success;
}
}  // namespace

void SkAiAiQueryStatus(CefRefPtr<CefBrowser> browser) {
  const bool started = LaunchWorker([browser]() {
    if (g_shutting_down.load()) return;
    const auto model = GetModelPath();
    std::error_code error;
    if (!std::filesystem::exists(model, error) || error) {
      NotifyStatus(browser, L"missing", 0,
                   L"Модель не установлена · загрузка около 1,3 ГБ");
      return;
    }
    bool verified = false;
    {
      std::lock_guard<std::mutex> lock(g_model_mutex);
      verified = g_model_verified_this_run;
    }
    if (!verified) {
      NotifyStatus(browser, L"checking", 0,
                   L"Проверка SHA-256 локальной модели…");
      verified = VerifyModel(model);
      if (verified) {
        std::lock_guard<std::mutex> lock(g_model_mutex);
        g_model_verified_this_run = true;
      }
    }
    NotifyStatus(browser, verified ? L"ready" : L"invalid",
                 verified ? 100 : 0,
                 verified ? L"Qwen 3.5 2B готова к запуску."
                          : L"Файл модели повреждён. Установите его заново.");
  });
  if (!started) NotifyStatus(browser, L"error", 0, L"Не удалось запустить фоновую задачу проверки модели.");
}

void SkAiAiInstallModel(CefRefPtr<CefBrowser> browser) {
  {
    std::lock_guard<std::mutex> lock(g_model_mutex);
    if (g_model_download_active) {
      NotifyStatus(browser, L"downloading", 0,
                   L"Загрузка модели уже выполняется.");
      return;
    }
    g_model_download_active = true;
  }

  NotifyStatus(browser, L"downloading", 0,
               L"Подготовка загрузки модели Qwen 3.5 2B…");
  const bool started = LaunchWorker([browser]() {
    const bool installed = DownloadModel(browser);
    {
      std::lock_guard<std::mutex> lock(g_model_mutex);
      g_model_download_active = false;
      if (!installed) g_model_verified_this_run = false;
    }
  });
  if (!started) {
    { std::lock_guard<std::mutex> lock(g_model_mutex); g_model_download_active = false; }
    NotifyStatus(browser, L"error", 0, L"Не удалось запустить фоновую загрузку модели.");
  }
}

void SkAiAiSendChat(CefRefPtr<CefBrowser> browser,
                    const std::wstring& request_id,
                    const std::wstring& openai_request_json) {
  if (openai_request_json.empty() ||
      openai_request_json.size() > kMaxRequestBytes ||
      openai_request_json.front() != L'{' ||
      openai_request_json.back() != L'}') {
    Notify(browser, L"localAiResult",
           JsonQuote(request_id) + L"," + JsonQuote(L"Некорректный формат запроса.") + L",true");
    return;
  }

  const bool started = LaunchWorker([browser, request_id, request = openai_request_json]() {
    if (g_shutting_down.load()) return;
    NotifyStatus(browser, L"loading", 0,
                 L"Запуск локальной модели на процессоре…");
    unsigned short port = 0;
    std::wstring api_key;
    std::wstring error_message;
    if (!EnsureServer(port, api_key, error_message)) {
      Notify(browser, L"localAiResult",
             JsonQuote(request_id) + L"," + JsonQuote(error_message) + L",true");
      NotifyStatus(browser, L"error", 0, error_message);
      return;
    }

    std::wstring answer;
    if (!PostChatRequest(port, api_key, request, answer, error_message)) {
      Notify(browser, L"localAiResult",
             JsonQuote(request_id) + L"," + JsonQuote(error_message) + L",true");
      NotifyStatus(browser, L"error", 0, error_message);
      return;
    }
    Notify(browser, L"localAiResult",
           JsonQuote(request_id) + L"," + JsonQuote(answer) + L",false");
    NotifyStatus(browser, L"ready", 100,
                 L"Qwen 3.5 2B · локальный режим · CPU");
  });
  if (!started) {
    Notify(browser, L"localAiResult", JsonQuote(request_id) + L"," +
           JsonQuote(L"Не удалось запустить фоновую задачу локального ИИ.") + L",true");
    NotifyStatus(browser, L"error", 0, L"Не удалось запустить фоновую задачу локального ИИ.");
  }
}

void SkAiAiShutdown() {
  g_shutting_down.store(true);

  {
    std::lock_guard<std::mutex> lock(g_server_mutex);
    if (g_server_process) {
      if (ProcessIsRunning(g_server_process)) {
        TerminateProcess(g_server_process, 0);
        WaitForSingleObject(g_server_process, 3000);
      }
      CloseHandle(g_server_process);
      g_server_process = nullptr;
    }
    if (g_server_job) {
      CloseHandle(g_server_job);
      g_server_job = nullptr;
    }
    g_server_port = 0;
    g_server_api_key.clear();
  }

  std::unique_lock<std::mutex> lock(g_worker_mutex);
  g_worker_condition.wait_for(lock, std::chrono::seconds(8),
                              [] { return g_active_workers == 0; });
}
