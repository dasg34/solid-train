// NOTE: The exact Tizen.Flutter.Embedding API may differ from this pseudocode.
// Verify against the actual SDK at build time. Key areas to check:
// - MethodChannel constructor and registration with Flutter engine
// - SetMethodCallHandler signature
// - ReceivedAppControl.ExtraData access pattern
//
// This file serves as the reference implementation for the AppControl → Flutter
// MethodChannel bridge. Adapt to the actual Tizen .NET Flutter embedding API.

using Tizen.Flutter.Embedding;

namespace OpenClawTvGenUI
{
    public class Runner : FlutterApplication
    {
        const string ChannelName = "openclaw/a2ui";
        private MethodChannel _channel;
        private string _pendingPath;
        private bool _flutterReady = false;

        protected override void OnCreate()
        {
            base.OnCreate();
            _channel = new MethodChannel(ChannelName);

            // Listen for Flutter "ready" signal to flush any pending path.
            _channel.SetMethodCallHandler((method, args) =>
            {
                if (method == "ready")
                {
                    _flutterReady = true;
                    if (_pendingPath != null)
                    {
                        _channel.InvokeMethod("loadFile", _pendingPath);
                        _pendingPath = null;
                    }
                }
            });

            // Store initial AppControl path (Flutter may not be ready yet).
            var filePath = ReceivedAppControl?.ExtraData?.TryGet("file");
            if (!string.IsNullOrEmpty(filePath))
            {
                _pendingPath = filePath;
            }
        }

        protected override void OnAppControlReceived(AppControlReceivedEventArgs e)
        {
            base.OnAppControlReceived(e);
            var filePath = e.ReceivedAppControl?.ExtraData?.TryGet("file");
            if (string.IsNullOrEmpty(filePath)) return;

            if (_flutterReady)
            {
                _channel.InvokeMethod("loadFile", filePath);
            }
            else
            {
                _pendingPath = filePath;
            }
        }
    }
}
