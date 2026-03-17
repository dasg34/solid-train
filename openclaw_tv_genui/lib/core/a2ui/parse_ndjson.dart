import 'dart:convert';

import 'package:genui/genui.dart';

/// Parses an NDJSON string into a list of [A2uiMessage].
List<A2uiMessage> parseNdjson(String ndjson) {
  final normalized = ndjson.trimLeft().replaceFirst(_utf8Bom, '');
  if (normalized.trim().isEmpty) {
    return const <A2uiMessage>[];
  }

  final wholeJson = _tryDecodeWholeJson(normalized);
  if (wholeJson != null) {
    if (wholeJson is List) {
      return wholeJson
          .map((entry) => _decodeMessageMap(entry, source: 'JSON array entry'))
          .toList();
    }

    if (wholeJson is Map<String, dynamic>) {
      if (_looksLikeA2uiEnvelope(wholeJson)) {
        return <A2uiMessage>[
          A2uiMessage.fromJson(wholeJson.cast<String, Object?>()),
        ];
      }

      throw const FormatException(
        'Expected A2UI message JSON, but received a normalized scenario payload. '
        'Pass the generated A2UI NDJSON output instead of the dump_normalized JSON file.',
      );
    }

    throw FormatException(
      'Expected A2UI JSON object or array, but got ${wholeJson.runtimeType}.',
    );
  }

  return normalized
      .split('\n')
      .where((line) => line.trim().isNotEmpty)
      .map((line) => _decodeMessageMap(jsonDecode(line), source: 'NDJSON line'))
      .toList();
}

dynamic _tryDecodeWholeJson(String source) {
  try {
    return jsonDecode(source);
  } catch (_) {
    return null;
  }
}

A2uiMessage _decodeMessageMap(dynamic jsonValue, {required String source}) {
  if (jsonValue is! Map<String, dynamic>) {
    throw FormatException(
      'Expected $source to decode to a JSON object, but got ${jsonValue.runtimeType}.',
    );
  }

  return A2uiMessage.fromJson(jsonValue.cast<String, Object?>());
}

bool _looksLikeA2uiEnvelope(Map<String, dynamic> json) {
  const messageKeys = <String>{
    'createSurface',
    'updateDataModel',
    'updateComponents',
    'deleteSurface',
  };

  return json.containsKey('version') || messageKeys.any(json.containsKey);
}

const String _utf8Bom = '\uFEFF';
