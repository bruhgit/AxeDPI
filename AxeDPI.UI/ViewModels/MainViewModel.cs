using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using Microsoft.Win32;
using AxeDPI.UI.Services;

namespace AxeDPI.UI.ViewModels
{
    public class MainViewModel : ViewModelBase
    {
        private readonly DpiService _dpiService;

        private bool _isRunning;
        private bool _isPaused;
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

        private string _newExcludedAppText = "";

        public ObservableCollection<string> LogMessages { get; } = new();
        public ObservableCollection<string> ExcludedApps { get; } = new();

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

        public bool IsPaused
        {
            get => _isPaused;
            set
            {
                if (SetProperty(ref _isPaused, value))
                {
                    OnPropertyChanged(nameof(StatusHeader));
                    OnPropertyChanged(nameof(StatusDescription));
                    OnPropertyChanged(nameof(PauseActionText));
                }
            }
        }

        public string StatusHeader => !IsRunning
            ? "Protection Disabled"
            : (IsPaused ? "DPI Bypass Paused" : "DPI Bypass Active");

        public string StatusDescription => !IsRunning
            ? "Direct unshielded connection • DPI filters active"
            : (IsPaused
                ? $"Proxy active on {_bindHost}:{_bindPort} • Direct passthrough (Bypass paused)"
                : $"Listening on {_bindHost}:{_bindPort} • Traffic is protected");

        public string ToggleActionText => IsRunning ? "Disconnect" : "Connect Now";
        public string PauseActionText => IsPaused ? "Resume DPI" : "Pause DPI";

        public string NewExcludedAppText
        {
            get => _newExcludedAppText;
            set => SetProperty(ref _newExcludedAppText, value);
        }

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
        public ICommand TogglePauseCommand { get; }
        public ICommand ToggleSystemProxyCommand { get; }
        public ICommand ToggleStartupCommand { get; }
        public ICommand LaunchDiscordCommand { get; }
        public ICommand TestDohCommand { get; }
        public ICommand ClearLogsCommand { get; }
        public ICommand AddExcludedAppCommand { get; }
        public ICommand RemoveExcludedAppCommand { get; }
        public ICommand BrowseAndAddAppCommand { get; }

        public MainViewModel()
        {
            _dpiService = DpiService.Instance;

            ToggleServiceCommand = new RelayCommand(ExecuteToggleService);
            TogglePauseCommand = new RelayCommand(ExecuteTogglePause);
            ToggleSystemProxyCommand = new RelayCommand(ExecuteToggleSystemProxy);
            ToggleStartupCommand = new RelayCommand(ExecuteToggleStartup);
            LaunchDiscordCommand = new RelayCommand(ExecuteLaunchDiscord);
            TestDohCommand = new RelayCommand(ExecuteTestDoh);
            ClearLogsCommand = new RelayCommand(() => LogMessages.Clear());
            AddExcludedAppCommand = new RelayCommand(ExecuteAddExcludedApp);
            RemoveExcludedAppCommand = new RelayCommand(param => ExecuteRemoveExcludedApp(param as string));
            BrowseAndAddAppCommand = new RelayCommand(ExecuteBrowseAndAddApp);

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

            _dpiService.OnPausedStateChanged += state =>
            {
                System.Windows.Application.Current?.Dispatcher?.BeginInvoke(() =>
                {
                    IsPaused = state;
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
            IsPaused = _dpiService.IsPaused();

            // Load exclusions and synchronize with unmanaged core
            LoadExclusions();
            _dpiService.SetExcludedApps(ExcludedApps);

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

        private void ExecuteTogglePause()
        {
            try
            {
                bool newState = !IsPaused;
                _dpiService.SetPaused(newState);
                IsPaused = _dpiService.IsPaused();
                AddLog(IsPaused
                    ? "DPI Bypass PAUSED: Direct passthrough active (packet fragmentation skipped)."
                    : "DPI Bypass RESUMED: Traffic protection and packet fragmentation active.");
            }
            catch (Exception ex)
            {
                AddLog($"[Error] Failed to toggle pause: {ex.Message}");
            }
        }

        private void ExecuteAddExcludedApp()
        {
            string val = NewExcludedAppText?.Trim() ?? "";
            if (string.IsNullOrWhiteSpace(val)) return;

            string fileName = Path.GetFileName(val);
            if (!fileName.EndsWith(".exe", StringComparison.OrdinalIgnoreCase))
            {
                fileName += ".exe";
            }

            if (!ExcludedApps.Any(x => string.Equals(x, fileName, StringComparison.OrdinalIgnoreCase)))
            {
                ExcludedApps.Add(fileName);
                SaveExclusions();
                _dpiService.SetExcludedApps(ExcludedApps);
                AddLog($"Added to DPI exclusions: {fileName}");
            }
            NewExcludedAppText = "";
        }

        private void ExecuteRemoveExcludedApp(string? appName)
        {
            if (string.IsNullOrWhiteSpace(appName)) return;

            var match = ExcludedApps.FirstOrDefault(x => string.Equals(x, appName, StringComparison.OrdinalIgnoreCase));
            if (match != null)
            {
                ExcludedApps.Remove(match);
                SaveExclusions();
                _dpiService.SetExcludedApps(ExcludedApps);
                AddLog($"Removed from DPI exclusions: {match}");
            }
        }

        private void ExecuteBrowseAndAddApp()
        {
            try
            {
                var dlg = new Microsoft.Win32.OpenFileDialog
                {
                    Title = "Select Executable to Exclude from DPI Bypass",
                    Filter = "Executable Files (*.exe)|*.exe|All Files (*.*)|*.*",
                    CheckFileExists = true
                };

                if (dlg.ShowDialog() == true)
                {
                    string fileName = Path.GetFileName(dlg.FileName);
                    if (!ExcludedApps.Any(x => string.Equals(x, fileName, StringComparison.OrdinalIgnoreCase)))
                    {
                        ExcludedApps.Add(fileName);
                        SaveExclusions();
                        _dpiService.SetExcludedApps(ExcludedApps);
                        AddLog($"Added to DPI exclusions: {fileName}");
                    }
                }
            }
            catch (Exception ex)
            {
                AddLog($"[Error] Failed to select file: {ex.Message}");
            }
        }

        private static string GetExclusionsFilePath()
        {
            string baseDir = AppDomain.CurrentDomain.BaseDirectory;
            string localPath = Path.Combine(baseDir, "exclusions.json");
            try
            {
                string testFile = Path.Combine(baseDir, ".testwrite");
                File.WriteAllText(testFile, "1");
                File.Delete(testFile);
                return localPath;
            }
            catch
            {
                string appData = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "AxeDPI");
                Directory.CreateDirectory(appData);
                return Path.Combine(appData, "exclusions.json");
            }
        }

        private void LoadExclusions()
        {
            try
            {
                string path = GetExclusionsFilePath();
                if (File.Exists(path))
                {
                    string json = File.ReadAllText(path);
                    var items = JsonSerializer.Deserialize<List<string>>(json);
                    if (items != null)
                    {
                        ExcludedApps.Clear();
                        foreach (var item in items)
                        {
                            if (!string.IsNullOrWhiteSpace(item))
                            {
                                ExcludedApps.Add(item.Trim());
                            }
                        }
                        return;
                    }
                }

                // Default exclusions for common apps sensitive to proxy TLS segmentation
                var defaults = new[] { "spotify.exe", "riotclient.exe", "valorant.exe", "leagueclient.exe" };
                ExcludedApps.Clear();
                foreach (var app in defaults)
                {
                    ExcludedApps.Add(app);
                }
                SaveExclusions();
            }
            catch (Exception ex)
            {
                AddLog($"[Exclusions] Error loading exclusions: {ex.Message}");
            }
        }

        private void SaveExclusions()
        {
            try
            {
                string path = GetExclusionsFilePath();
                var list = new List<string>(ExcludedApps);
                string json = JsonSerializer.Serialize(list, new JsonSerializerOptions { WriteIndented = true });
                File.WriteAllText(path, json);
            }
            catch (Exception ex)
            {
                AddLog($"[Exclusions] Error saving exclusions: {ex.Message}");
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
