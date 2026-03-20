import 'dart:convert';

import 'package:genui/genui.dart';

import 'presentation_a2ui_builder.dart';
import 'presentation_surface.dart';

PresentationSurface decodePresentationSurface(
  String raw, {
  String sourceLabel = 'payload',
}) {
  final decoded = jsonDecode(raw);
  if (decoded is! Map) {
    throw FormatException(
      'Presentation $sourceLabel must decode to a JSON object.',
    );
  }

  return PresentationSurface.fromJson(Map<String, Object?>.from(decoded));
}

List<A2uiMessage> decodePresentationMessages(
  String raw, {
  String sourceLabel = 'payload',
}) {
  final surface = decodePresentationSurface(raw, sourceLabel: sourceLabel);
  return buildPresentationMessages(surface);
}
