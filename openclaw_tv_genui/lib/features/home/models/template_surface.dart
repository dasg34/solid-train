import 'package:genui/genui.dart';

enum TemplateSurfaceStyle {
  standard,
  atmosphericWeather,
  newsPanel,
  schedulePanel,
}

class TemplateSurfacePayload {
  const TemplateSurfacePayload({
    required this.components,
    this.dataModel = const <String, Object?>{},
    this.surfaceStyle = TemplateSurfaceStyle.standard,
  });

  final List<Component> components;
  final Map<String, Object?> dataModel;
  final TemplateSurfaceStyle surfaceStyle;
}

abstract interface class TemplateSurfacePresenter {
  TemplateSurfaceStyle get surfaceStyle;

  TemplateSurfacePayload? buildLoading();

  TemplateSurfacePayload buildError(Object error);

  Future<TemplateSurfacePayload> load();
}

class StaticTemplateSurfacePresenter implements TemplateSurfacePresenter {
  const StaticTemplateSurfacePresenter({
    required this.components,
    this.dataModel = const <String, Object?>{},
    this.surfaceStyle = TemplateSurfaceStyle.standard,
  });

  final List<Component> components;
  final Map<String, Object?> dataModel;

  @override
  final TemplateSurfaceStyle surfaceStyle;

  @override
  TemplateSurfacePayload? buildLoading() => null;

  @override
  TemplateSurfacePayload buildError(Object error) {
    return TemplateSurfacePayload(
      components: buildTemplateStatusComponents(),
      dataModel: buildTemplateStatusModel(
        title: '템플릿을 표시하지 못했습니다',
        detail: '정적 템플릿 렌더링에 실패했습니다.',
        hint: '$error',
      ),
      surfaceStyle: surfaceStyle,
    );
  }

  @override
  Future<TemplateSurfacePayload> load() async {
    return TemplateSurfacePayload(
      components: components,
      dataModel: dataModel,
      surfaceStyle: surfaceStyle,
    );
  }
}

List<Component> buildTemplateStatusComponents() {
  return [
    _component('root', 'Column', {
      'distribution': 'center',
      'alignment': 'stretch',
      'children': ['statusCard'],
    }),
    _component('statusCard', 'Card', {'child': 'statusColumn'}),
    _component('statusColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['statusTitle', 'statusDetail', 'statusHint'],
    }),
    _textPath('statusTitle', '/title', usageHint: 'h1'),
    _textPath('statusDetail', '/detail', usageHint: 'h4'),
    _textPath('statusHint', '/hint'),
  ];
}

Map<String, Object?> buildTemplateStatusModel({
  required String title,
  required String detail,
  required String hint,
}) {
  return {
    'title': _cleanText(title, maxLen: 24),
    'detail': _cleanText(detail, maxLen: 80),
    'hint': _cleanText(hint, maxLen: 100),
  };
}

Component _component(
  String id,
  String type,
  Map<String, Object?> properties, {
  int? weight,
}) {
  return Component(
    id: id,
    componentProperties: {type: properties},
    weight: weight,
  );
}

Component _textPath(
  String id,
  String path, {
  String usageHint = 'body',
  int? weight,
}) {
  return Component(
    id: id,
    componentProperties: {
      'Text': {
        'text': {'path': path},
        'usageHint': usageHint,
      },
    },
    weight: weight,
  );
}

String _cleanText(Object? value, {int maxLen = 80}) {
  final text = '${value ?? ''}'.trim().replaceAll(RegExp(r'\s+'), ' ');
  if (text.length <= maxLen) {
    return text;
  }
  return '${text.substring(0, maxLen - 3).trimRight()}...';
}
