void logToPlatform({
  required String tag,
  required String level,
  required String message,
}) {
  // ignore: avoid_print
  print('[$tag][$level] $message');
}
