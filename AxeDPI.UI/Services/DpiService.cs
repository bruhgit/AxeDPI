using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace AxeDPI.UI.Services
{
    public class DpiService
    {
        private static DpiService? _instance;
        public static DpiService Instance => _instance ??= new DpiService();

        // Prevent GC collection of unmanaged callback delegate
        private readonly NativeMethods.DpiLogCallback _logCallbackDelegate;

        public event Action<string>? OnLogReceived;
        public event Action<bool>? OnRunningStateChanged;
        public event Action<bool>? OnSystemProxyStateChanged;
        public event Action<ulong, ulong, int>? OnStatsUpdated;

        private System.Threading.Timer? _statsTimer;

        private DpiService()
        {
            _logCallbackDelegate = HandleNativeLog;
            try
            {
                NativeMethods.DpiSetLogCallback(_logCallbackDelegate);
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Failed to set DPI log callback: {ex.Message}");
            }
        }

        private void HandleNativeLog(string message)
        {
            OnLogReceived?.Invoke(message);
        }

        public bool IsRunning()
        {
            try
            {
                return NativeMethods.DpiIsRunning() != 0;
            }
            catch
            {
                return false;
            }
        }

        public bool IsSystemProxyEnabled()
        {
            try
            {
                return NativeMethods.DpiIsSystemProxyEnabled() != 0;
            }
            catch
            {
                return false;
            }
        }

        public bool IsStartupEnabled()
        {
            try
            {
                return NativeMethods.DpiIsStartupEnabled() != 0;
            }
            catch
            {
                return false;
            }
        }

        public bool Start(string host, int port, int splitPos, int splitDelayMs, string? upstream, bool autoSysProxy)
        {
            try
            {
                int res = NativeMethods.DpiStartServer(host, port, splitPos, splitDelayMs, upstream, autoSysProxy ? 1 : 0);
                bool success = res != 0;

                if (success)
                {
                    StartStatsPolling();
                }

                OnRunningStateChanged?.Invoke(success);
                OnSystemProxyStateChanged?.Invoke(IsSystemProxyEnabled());
                return success;
            }
            catch (Exception ex)
            {
                OnLogReceived?.Invoke($"[Critical Error] Failed to invoke DPI native engine: {ex.Message}");
                OnRunningStateChanged?.Invoke(false);
                return false;
            }
        }

        public void Stop()
        {
            try
            {
                StopStatsPolling();
                NativeMethods.DpiStopServer();
                OnRunningStateChanged?.Invoke(false);
                OnSystemProxyStateChanged?.Invoke(false);
            }
            catch (Exception ex)
            {
                OnLogReceived?.Invoke($"[Error] Stop error: {ex.Message}");
            }
        }

        public bool SetSystemProxy(bool enable, string proxyAddr)
        {
            try
            {
                int res = NativeMethods.DpiSetSystemProxy(enable ? 1 : 0, proxyAddr);
                bool state = IsSystemProxyEnabled();
                OnSystemProxyStateChanged?.Invoke(state);
                return res != 0;
            }
            catch (Exception ex)
            {
                OnLogReceived?.Invoke($"[Error] Proxy error: {ex.Message}");
                return false;
            }
        }

        public bool SetStartup(bool enable)
        {
            try
            {
                string exePath = Environment.ProcessPath ?? "";
                return NativeMethods.DpiSetStartup(enable ? 1 : 0, exePath) != 0;
            }
            catch
            {
                return false;
            }
        }

        public bool LaunchDiscord(string proxyAddr)
        {
            return NativeMethods.DpiLaunchDiscordWithProxy(proxyAddr) != 0;
        }

        public async Task<(bool success, string ip, int latencyMs)> TestDoHAsync(string domain)
        {
            return await Task.Run(() =>
            {
                var sb = new StringBuilder(256);
                int latency = 0;
                int res = NativeMethods.DpiTestDoH(domain, sb, sb.Capacity, out latency);
                return (res != 0, sb.ToString(), latency);
            });
        }

        private void StartStatsPolling()
        {
            StopStatsPolling();
            _statsTimer = new System.Threading.Timer(_ =>
            {
                if (IsRunning())
                {
                    NativeMethods.DpiGetStats(out ulong bytesIn, out ulong bytesOut, out int activeConns);
                    OnStatsUpdated?.Invoke(bytesIn, bytesOut, activeConns);
                }
            }, null, 1000, 1000);
        }

        private void StopStatsPolling()
        {
            _statsTimer?.Dispose();
            _statsTimer = null;
        }
    }
}
