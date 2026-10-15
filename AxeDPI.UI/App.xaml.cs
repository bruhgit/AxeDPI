using System;
using System.Threading;
using System.Windows;

namespace AxeDPI.UI
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// Single-instance application enforcement with Mutex & EventWaitHandle
    /// </summary>
    public partial class App : System.Windows.Application
    {
        private const string MutexId = "Local\\AxeDPI_SingleInstance_Application_Mutex_v2";
        private const string EventWaitHandleId = "Local\\AxeDPI_SingleInstance_ShowEvent_v2";

        private static Mutex? _mutex;
        private static EventWaitHandle? _eventWaitHandle;
        private static Thread? _listenerThread;
        private static bool _isFirstInstance;

        protected override void OnStartup(StartupEventArgs e)
        {
            try
            {
                _mutex = new Mutex(true, MutexId, out _isFirstInstance);
            }
            catch (AbandonedMutexException)
            {
                // Previous process crashed and left the mutex; we now own it
                _isFirstInstance = true;
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Mutex creation failed: {ex.Message}");
                _isFirstInstance = true;
            }

            if (!_isFirstInstance)
            {
                // Second instance detected: Signal existing instance to restore window and bring to front
                try
                {
                    using var waitHandle = EventWaitHandle.OpenExisting(EventWaitHandleId);
                    waitHandle.Set();
                }
                catch
                {
                }

                // Immediately terminate the second instance
                Environment.Exit(0);
                return;
            }

            // Primary instance: Create the EventWaitHandle and listen for secondary launches
            try
            {
                _eventWaitHandle = new EventWaitHandle(false, EventResetMode.AutoReset, EventWaitHandleId);
                _listenerThread = new Thread(ListenForOtherInstances)
                {
                    IsBackground = true,
                    Name = "AxeDPI_SingleInstanceListener"
                };
                _listenerThread.Start();
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Failed to create SingleInstance EventWaitHandle: {ex.Message}");
            }

            base.OnStartup(e);
        }

        private void ListenForOtherInstances()
        {
            while (_eventWaitHandle != null)
            {
                try
                {
                    if (_eventWaitHandle.WaitOne())
                    {
                        Dispatcher.Invoke(() =>
                        {
                            if (MainWindow is MainWindow mw)
                            {
                                mw.RestoreWindow();
                            }
                        });
                    }
                }
                catch (ObjectDisposedException)
                {
                    break;
                }
                catch (AbandonedMutexException)
                {
                    break;
                }
                catch
                {
                    // Catch-all to keep the listener loop alive
                }
            }
        }

        protected override void OnExit(ExitEventArgs e)
        {
            try
            {
                _eventWaitHandle?.Dispose();
                _eventWaitHandle = null;

                if (_mutex != null)
                {
                    if (_isFirstInstance)
                    {
                        try
                        {
                            _mutex.ReleaseMutex();
                        }
                        catch
                        {
                        }
                    }
                    _mutex.Dispose();
                    _mutex = null;
                }
            }
            catch
            {
            }

            base.OnExit(e);
        }
    }
}

