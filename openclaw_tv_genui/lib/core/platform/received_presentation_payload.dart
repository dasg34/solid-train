class ReceivedPresentationPayload {
  const ReceivedPresentationPayload({this.filePath, this.rawJson});

  final String? filePath;
  final String? rawJson;

  bool get hasFilePath => filePath != null && filePath!.isNotEmpty;
  bool get hasRawJson => rawJson != null && rawJson!.trim().isNotEmpty;
}
