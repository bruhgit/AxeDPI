using System;
using System.ComponentModel;
using System.Windows;
using System.Windows.Input;
using AxeDPI.UI.Services;
using AxeDPI.UI.ViewModels;

namespace AxeDPI.UI
{
    public partial class MainWindow : Window
    {
        private TrayService? _trayService;

        public bool IsExplicitExit { get; set; } = false;

        public MainWindow()
        {
            InitializeComponent();
            Loaded += MainWindow_Loaded;
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            if (DataContext is MainViewModel vm)
            {
                _trayService = new TrayService(this, vm);
            }
        }

        private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ButtonState == MouseButtonState.Pressed)
            {
                DragMove();
            }
        }

        private void BtnMinimize_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void BtnClose_Click(object sender, RoutedEventArgs e)
        {
            // Minimize to tray on close button click
            if (_trayService != null)
            {
                _trayService.MinimizeToTray();
            }
            else
            {
                Hide();
            }
        }

        protected override void OnClosing(CancelEventArgs e)
        {
            if (!IsExplicitExit)
            {
                e.Cancel = true;
                if (_trayService != null)
                {
                    _trayService.MinimizeToTray();
                }
                else
                {
                    Hide();
                }
                return;
            }

            base.OnClosing(e);
            _trayService?.Dispose();
            _trayService = null;
        }

        public void ExitGracefully()
        {
            IsExplicitExit = true;
            if (DataContext is MainViewModel vm && vm.IsRunning)
            {
                vm.ToggleServiceCommand.Execute(null);
            }

            _trayService?.Dispose();
            _trayService = null;
            System.Windows.Application.Current.Shutdown();
        }

        public void RestoreWindow()
        {
            if (_trayService != null)
            {
                _trayService.RestoreWindow();
            }
            else
            {
                Show();
                if (WindowState == WindowState.Minimized)
                {
                    WindowState = WindowState.Normal;
                }
                Activate();
                Topmost = true;
                Topmost = false;
                Focus();
            }

            try
            {
                var helper = new System.Windows.Interop.WindowInteropHelper(this);
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
    }
}