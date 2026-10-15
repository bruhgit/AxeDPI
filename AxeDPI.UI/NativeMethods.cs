using System;
using System.Runtime.InteropServices;
using System.Text;

namespace AxeDPI.UI
{
    public static class NativeMethods
    {
        private const string DllName = "AxeDPI.Service.DllComponents.dll";

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void DpiLogCallback([MarshalAs(UnmanagedType.LPStr)] string message);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int DpiStartServer(
            [MarshalAs(UnmanagedType.LPStr)] string bindHost,
            int bindPort,
            int splitPos,
            int splitDelayMs,
            [MarshalAs(UnmanagedType.LPStr)] string? upstreamProxy,
            int autoSysProxy);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void DpiStopServer();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int DpiIsRunning();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void DpiSetPaused(int paused);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int DpiIsPaused();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern void DpiSetExcludedApps([MarshalAs(UnmanagedType.LPStr)] string commaSeparatedList);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int DpiIsAppExcluded([MarshalAs(UnmanagedType.LPStr)] string appName);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int DpiSetSystemProxy(
            int enable,
            [MarshalAs(UnmanagedType.LPStr)] string proxyAddr);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int DpiIsSystemProxyEnabled();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int DpiSetStartup(
            int enable,
            [MarshalAs(UnmanagedType.LPStr)] string? exePath);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int DpiIsStartupEnabled();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int DpiLaunchDiscordWithProxy(
            [MarshalAs(UnmanagedType.LPStr)] string proxyAddr);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int DpiTestDoH(
            [MarshalAs(UnmanagedType.LPStr)] string testDomain,
            [MarshalAs(UnmanagedType.LPStr)] StringBuilder outIpBuffer,
            int bufferLen,
            out int outLatencyMs);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void DpiGetStats(
            out ulong bytesIn,
            out ulong bytesOut,
            out int activeConnections);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void DpiSetLogCallback(DpiLogCallback callback);

        // Win32 User32 APIs for single-instance window restoring
        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool SetForegroundWindow(IntPtr hWnd);

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);

        public const int SW_RESTORE = 9;
        public const int SW_SHOW = 5;
    }
}
