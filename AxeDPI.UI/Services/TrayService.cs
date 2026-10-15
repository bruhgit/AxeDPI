using System;
using System.Drawing;
using System.IO;
using System.Windows.Forms;
using AxeDPI.UI.ViewModels;

namespace AxeDPI.UI.Services
{
    public class TrayService : IDisposable
    {
        private readonly NotifyIcon _notifyIcon;
        private readonly ContextMenuStrip _contextMenu;
        private readonly ToolStripMenuItem _itemStatus;
        private readonly ToolStripMenuItem _itemPause;
        private readonly ToolStripMenuItem _itemProxy;
        private readonly System.Windows.Window _mainWindow;
        private readonly MainViewModel _viewModel;

        public TrayService(System.Windows.Window mainWindow, MainViewModel viewModel)
        {
            _mainWindow = mainWindow ?? throw new ArgumentNullException(nameof(mainWindow));
            _viewModel = viewModel ?? throw new ArgumentNullException(nameof(viewModel));

            _contextMenu = new ContextMenuStrip();
            _contextMenu.RenderMode = ToolStripRenderMode.System;

            // 1. Show App
            var itemShow = new ToolStripMenuItem("Open AxeDPI", null, (s, e) => RestoreWindow());
            itemShow.Font = new Font(itemShow.Font, FontStyle.Bold);
            _contextMenu.Items.Add(itemShow);

            var itemHide = new ToolStripMenuItem("Hide to Tray", null, (s, e) => MinimizeToTray());
            _contextMenu.Items.Add(itemHide);

            _contextMenu.Items.Add(new ToolStripSeparator());

            // 2. Toggle DPI Bypass
            _itemStatus = new ToolStripMenuItem("DPI Bypass: Disabled", null, (s, e) =>
            {
                _viewModel.ToggleServiceCommand.Execute(null);
            });
            _contextMenu.Items.Add(_itemStatus);

            // 2b. Toggle DPI Pause
            _itemPause = new ToolStripMenuItem("Pause DPI", null, (s, e) =>
            {
                _viewModel.TogglePauseCommand.Execute(null);
            });
            _contextMenu.Items.Add(_itemPause);

            // 3. Toggle System Proxy
            _itemProxy = new ToolStripMenuItem("System Proxy: Disabled", null, (s, e) =>
            {
                _viewModel.ToggleSystemProxyCommand.Execute(null);
            });
            _contextMenu.Items.Add(_itemProxy);

            // 4. Launch Discord
            var itemDiscord = new ToolStripMenuItem("Launch Discord", null, (s, e) =>
            {
                _viewModel.LaunchDiscordCommand.Execute(null);
            });
            _contextMenu.Items.Add(itemDiscord);

            _contextMenu.Items.Add(new ToolStripSeparator());

            // 5. Exit
            var itemExit = new ToolStripMenuItem("Exit AxeDPI", null, (s, e) =>
            {
                ExitApplication();
            });
            _contextMenu.Items.Add(itemExit);

            // Initialize NotifyIcon
            _notifyIcon = new NotifyIcon
            {
                Text = "AxeDPI",
                ContextMenuStrip = _contextMenu,
                Visible = true
            };

            // Load Icon
            try
            {
                string iconPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Assets", "app.ico");
                if (File.Exists(iconPath))
                {
                    _notifyIcon.Icon = new Icon(iconPath);
                }
                else
                {
                    _notifyIcon.Icon = Icon.ExtractAssociatedIcon(Environment.ProcessPath ?? "") ?? SystemIcons.Application;
                }
            }
            catch
            {
                _notifyIcon.Icon = SystemIcons.Application;
            }

            _notifyIcon.DoubleClick += (s, e) => RestoreWindow();
            _notifyIcon.Click += (s, e) =>
            {
                if (e is MouseEventArgs mouseArgs && mouseArgs.Button == MouseButtons.Left)
                {
                    RestoreWindow();
                }
            };

            // Wire up ViewModel changes to tray text
            _viewModel.PropertyChanged += (s, e) =>
            {
                if (e.PropertyName == nameof(MainViewModel.IsRunning) ||
                    e.PropertyName == nameof(MainViewModel.IsPaused) ||
                    e.PropertyName == nameof(MainViewModel.IsSystemProxyActive))
                {
                    UpdateTrayState();
                }
            };

            UpdateTrayState();
        }

        public void UpdateTrayState()
        {
            bool running = _viewModel.IsRunning;
            bool paused = _viewModel.IsPaused;
            bool proxy = _viewModel.IsSystemProxyActive;

            _itemStatus.Text = running ? "DPI Bypass: Active" : "DPI Bypass: Disabled";
            _itemStatus.Checked = running;

            _itemPause.Enabled = running;
            if (!running)
            {
                _itemPause.Text = "Pause DPI";
                _itemPause.Checked = false;
            }
            else if (paused)
            {
                _itemPause.Text = "Resume DPI (Paused)";
                _itemPause.Checked = true;
            }
            else
            {
                _itemPause.Text = "Pause DPI";
                _itemPause.Checked = false;
            }

            _itemProxy.Text = proxy ? "System Proxy: Active" : "System Proxy: Disabled";
            _itemProxy.Checked = proxy;

            string status = running ? (paused ? "Paused" : "Active") : "Disabled";
            string tooltip = $"AxeDPI ({status})";
            if (tooltip.Length > 63) tooltip = tooltip.Substring(0, 63);
            _notifyIcon.Text = tooltip;
        }

        public void MinimizeToTray()
        {
            _mainWindow.Hide();
            _notifyIcon.ShowBalloonTip(1500, "AxeDPI", "AxeDPI is running in the background system tray.", ToolTipIcon.Info);
        }

        public void RestoreWindow()
        {
            _mainWindow.Show();
            if (_mainWindow.WindowState == System.Windows.WindowState.Minimized)
            {
                _mainWindow.WindowState = System.Windows.WindowState.Normal;
            }
            _mainWindow.Activate();
            _mainWindow.Topmost = true;
            _mainWindow.Topmost = false;
            _mainWindow.Focus();

            try
            {
                var helper = new System.Windows.Interop.WindowInteropHelper(_mainWindow);
                if (helper.Handle != IntPtr.Zero)
                {
                    NativeMethods.ShowWindowAsync(helper.Handle, NativeMethods.SW_RESTORE);
                    NativeMethods.SetForegroundWindow(helper.Handle);
                }
            }
            catch
            {
            }
        }

        public void ExitApplication()
        {
            if (_mainWindow is MainWindow mw)
            {
                mw.ExitGracefully();
            }
            else
            {
                if (_viewModel.IsRunning)
                {
                    _viewModel.ToggleServiceCommand.Execute(null);
                }
                _notifyIcon.Visible = false;
                _notifyIcon.Dispose();
                System.Windows.Application.Current.Shutdown();
            }
        }

        public void Dispose()
        {
            _notifyIcon.Visible = false;
            _notifyIcon.Dispose();
            _contextMenu.Dispose();
        }
    }
}
