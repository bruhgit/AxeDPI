using System;
using System.Collections.ObjectModel;
using System.Windows;
using System.Windows.Input;
using AxeDPI.UI.Services;

namespace AxeDPI.UI.ViewModels
{
    public class MainViewModel : ViewModelBase
    {
        private readonly DpiService _dpiService;

        private bool _isRunning;
        private bool _isSystemProxyActive;
        private bool _isStartupActive;
        private string _bindHost = "127.0.0.1";
        private int _bindPort = 10808;
        private int _splitPos = 40;
        private int _splitDelayMs = 10;
        private string _upstreamProxy = "";
        private bool _autoSystemProxy = true;

        private int _activeConnections;
        private string _bytesDownloadedStr = "0 KB";
        private string _bytesUploadedStr = "0 KB";

        private string _dohTestDomain = "discord.com";
        private string _dohTestResult = "DoH Ready (Click Test to ping)";
        private bool _isTestingDoh;

        public ObservableCollection<string> LogMessages { get; } = new();

        public bool IsRunning
        {
            get => _isRunning;
            set
            {
                if (SetProperty(ref _isRunning, value))
                {
                    OnPropertyChanged(nameof(StatusHeader));
                    OnPropertyChanged(nameof(StatusDescription));
                    OnPropertyChanged(nameof(ToggleActionText));
                }
            }
        }

        public string StatusHeader => IsRunning ? "DPI Bypass Active" : "Protection Disabled";
        public string StatusDescription => IsRunning
            ? $"Listening on {_bindHost}:{_bindPort} • Traffic is protected"
            : "Direct unshielded connection • DPI filters active";

        public string ToggleActionText => IsRunning ? "Disconnect" : "Connect Now";

        public bool IsSystemProxyActive
        {
            get => _isSystemProxyActive;
            set => SetProperty(ref _isSystemProxyActive, value);
        }

        public bool IsStartupActive
        {
            get => _isStartupActive;
            set => SetProperty(ref _isStartupActive, value);
        }

        public string BindHost
        {
            get => _bindHost;
            set => SetProperty(ref _bindHost, value);
        }

        public int BindPort
        {
            get => _bindPort;
            set => SetProperty(ref _bindPort, value);
        }

        public int SplitPos
        {
            get => _splitPos;
            set => SetProperty(ref _splitPos, value);
        }

        public int SplitDelayMs
        {
            get => _splitDelayMs;
            set => SetProperty(ref _splitDelayMs, value);
        }

        public string UpstreamProxy
        {
            get => _upstreamProxy;
            set => SetProperty(ref _upstreamProxy, value);
        }

        public bool AutoSystemProxy
        {
            get => _autoSystemProxy;
            set => SetProperty(ref _autoSystemProxy, value);
        }

        public int ActiveConnections
        {
            get => _activeConnections;
            set => SetProperty(ref _activeConnections, value);
        }

        public string BytesDownloadedStr
        {
            get => _bytesDownloadedStr;
            set => SetProperty(ref _bytesDownloadedStr, value);
        }

        public string BytesUploadedStr
        {
            get => _bytesUploadedStr;
            set => SetProperty(ref _bytesUploadedStr, value);
        }

        public string DohTestDomain
        {
            get => _dohTestDomain;
            set => SetProperty(ref _dohTestDomain, value);
        }

        public string DohTestResult
        {
            get => _dohTestResult;
            set => SetProperty(ref _dohTestResult, value);
        }

        public bool IsTestingDoh
        {
            get => _isTestingDoh;
            set => SetProperty(ref _isTestingDoh, value);
        }

        public ICommand ToggleServiceCommand { get; }
        public ICommand ToggleSystemProxyCommand { get; }
        public ICommand ToggleStartupCommand { get; }
        public ICommand LaunchDiscordCommand { get; }
        public ICommand TestDohCommand { get; }
        public ICommand ClearLogsCommand { get; }

        public MainViewModel()
        {
            _dpiService = DpiService.Instance;

            ToggleServiceCommand = new RelayCommand(ExecuteToggleService);
            ToggleSystemProxyCommand = new RelayCommand(ExecuteToggleSystemProxy);
            ToggleStartupCommand = new RelayCommand(ExecuteToggleStartup);
            LaunchDiscordCommand = new RelayCommand(ExecuteLaunchDiscord);
            TestDohCommand = new RelayCommand(ExecuteTestDoh);
            ClearLogsCommand = new RelayCommand(() => LogMessages.Clear());

            _dpiService.OnLogReceived += msg =>
            {
                System.Windows.Application.Current?.Dispatcher?.BeginInvoke(() =>
                {
                    string time = DateTime.Now.ToString("HH:mm:ss");
                    LogMessages.Add($"[{time}] {msg}");
                    if (LogMessages.Count > 400)
                    {
                        LogMessages.RemoveAt(0);
                    }
                });
            };

            _dpiService.OnRunningStateChanged += state =>
            {
                System.Windows.Application.Current?.Dispatcher?.BeginInvoke(() =>
                {
                    IsRunning = state;
                });
            };

            _dpiService.OnSystemProxyStateChanged += state =>
            {
                System.Windows.Application.Current?.Dispatcher?.BeginInvoke(() =>
                {
                    IsSystemProxyActive = state;
                });
            };

            _dpiService.OnStatsUpdated += (bytesIn, bytesOut, conns) =>
            {
                System.Windows.Application.Current?.Dispatcher?.BeginInvoke(() =>
                {
                    BytesDownloadedStr = FormatBytes(bytesIn);
                    BytesUploadedStr = FormatBytes(bytesOut);
                    ActiveConnections = conns;
                });
            };

            // Initial state inspection
            IsSystemProxyActive = _dpiService.IsSystemProxyEnabled();
            IsStartupActive = _dpiService.IsStartupEnabled();
            IsRunning = _dpiService.IsRunning();

            AddLog("AxeDPI initialized. Ready.");
        }

        private void AddLog(string msg)
        {
            string time = DateTime.Now.ToString("HH:mm:ss");
            LogMessages.Add($"[{time}] {msg}");
        }

        private async void ExecuteToggleService()
        {
            try
            {
                if (IsRunning)
                {
                    AddLog("Stopping proxy service...");
                    await Task.Run(() => _dpiService.Stop());
                    AddLog("Proxy service stopped by user.");
                }
                else
                {
                    AddLog("Starting DPI Proxy Service...");
                    string host = BindHost;
                    int port = BindPort;
                    int splitPos = SplitPos;
                    int splitDelay = SplitDelayMs;
                    string? upstream = string.IsNullOrWhiteSpace(UpstreamProxy) ? null : UpstreamProxy;
                    bool autoSysProxy = AutoSystemProxy;

                    bool started = await Task.Run(() => _dpiService.Start(
                        host,
                        port,
                        splitPos,
                        splitDelay,
                        upstream,
                        autoSysProxy));

                    if (started)
                    {
                        AddLog($"Proxy service started on {host}:{port} (Split offset: {splitPos})");
                    }
                    else
                    {
                        AddLog("Failed to start proxy service. Check port or permissions.");
                    }
                }
            }
            catch (Exception ex)
            {
                AddLog($"[Error] Service error: {ex.Message}");
            }
        }

        private async void ExecuteToggleSystemProxy()
        {
            try
            {
                bool newState = !IsSystemProxyActive;
                string proxyAddr = $"{BindHost}:{BindPort}";
                await Task.Run(() => _dpiService.SetSystemProxy(newState, proxyAddr));
                IsSystemProxyActive = _dpiService.IsSystemProxyEnabled();
                AddLog(IsSystemProxyActive ? "Windows System Proxy ENABLED." : "Windows System Proxy DISABLED.");
            }
            catch (Exception ex)
            {
                AddLog($"[Error] Proxy toggle error: {ex.Message}");
            }
        }

        private async void ExecuteToggleStartup()
        {
            try
            {
                bool newState = !IsStartupActive;
                await Task.Run(() => _dpiService.SetStartup(newState));
                IsStartupActive = _dpiService.IsStartupEnabled();
                AddLog(IsStartupActive ? "Windows Startup ENABLED." : "Windows Startup DISABLED.");
            }
            catch (Exception ex)
            {
                AddLog($"[Error] Startup toggle error: {ex.Message}");
            }
        }

        private void ExecuteLaunchDiscord()
        {
            string proxyAddr = $"{BindHost}:{BindPort}";
            bool ok = _dpiService.LaunchDiscord(proxyAddr);
            if (ok)
            {
                AddLog($"Discord launched with --proxy-server=http://{proxyAddr}");
            }
            else
            {
                AddLog("Failed to locate or launch Discord. Ensure Discord is installed.");
            }
        }

        private async void ExecuteTestDoh()
        {
            if (IsTestingDoh) return;
            IsTestingDoh = true;
            DohTestResult = "Querying Cloudflare/Google DoH...";

            try
            {
                var (success, ip, latency) = await _dpiService.TestDoHAsync(DohTestDomain);
                if (success)
                {
                    DohTestResult = $"Latency: {latency} ms • Resolved IP: {ip}";
                    AddLog($"[DoH Test] {DohTestDomain} -> {ip} in {latency}ms");
                }
                else
                {
                    DohTestResult = $"Test Failed: {ip}";
                    AddLog($"[DoH Test] Query failed for {DohTestDomain}");
                }
            }
            catch (Exception ex)
            {
                DohTestResult = $"Error: {ex.Message}";
                AddLog($"[DoH Test] Exception: {ex.Message}");
            }
            finally
            {
                IsTestingDoh = false;
            }
        }

        private static string FormatBytes(ulong bytes)
        {
            if (bytes < 1024) return $"{bytes} B";
            if (bytes < 1024 * 1024) return $"{bytes / 1024.0:F1} KB";
            if (bytes < 1024 * 1024 * 1024) return $"{bytes / (1024.0 * 1024.0):F2} MB";
            return $"{bytes / (1024.0 * 1024.0 * 1024.0):F2} GB";
        }
    }
}
