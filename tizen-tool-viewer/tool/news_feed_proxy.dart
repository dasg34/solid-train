import 'dart:convert';
import 'dart:io';

const _defaultHost = '127.0.0.1';
const _defaultPort = 3001;
const _yonhapFeedUrl = 'https://www.yonhapnewstv.co.kr/browse/feed/';
const _scheduleFeedUrl = 'https://holidays.hyunbin.page/basic.ics';
const _dailySkillDir =
    '/Users/yohoho/work/skills/tv-scenarios/tv-daily-briefing';
const _sportsSkillDir =
    '/Users/yohoho/work/skills/tv-scenarios/tv-sports-briefing';
const _financeSkillDir =
    '/Users/yohoho/work/skills/tv-scenarios/tv-finance-snapshot';
const _financeWatchlist = '005930:삼성전자,000660:SK하이닉스,035420:NAVER';

Future<void> main(List<String> args) async {
  final host = args.isNotEmpty ? args.first : _defaultHost;
  final port = args.length > 1
      ? int.tryParse(args[1]) ?? _defaultPort
      : _defaultPort;

  final server = await HttpServer.bind(host, port);
  stdout.writeln('TV data proxy listening on http://$host:$port');

  await for (final request in server) {
    try {
      await _handleRequest(request);
    } catch (error) {
      stderr.writeln('Proxy error: $error');
      _addCorsHeaders(request.response);
      request.response.statusCode = HttpStatus.internalServerError;
      request.response.headers.contentType = ContentType.json;
      request.response.write(
        jsonEncode({'error': 'internal_error', 'message': '$error'}),
      );
      await request.response.close();
    }
  }
}

Future<void> _handleRequest(HttpRequest request) async {
  _addCorsHeaders(request.response);

  if (request.method == 'OPTIONS') {
    request.response.statusCode = HttpStatus.noContent;
    await request.response.close();
    return;
  }

  if (request.method != 'GET') {
    await _writeNotFound(request.response);
    return;
  }

  switch (request.uri.path) {
    case '/yonhap-feed':
      await _proxyUpstreamText(
        request.response,
        url: _yonhapFeedUrl,
        contentType: 'application/rss+xml; charset=utf-8',
      );
      return;
    case '/schedule-feed':
      await _proxyUpstreamText(
        request.response,
        url: _scheduleFeedUrl,
        contentType: 'text/calendar; charset=utf-8',
      );
      return;
    case '/daily-briefing':
      await _proxyNormalizedSkill(
        request.response,
        workingDirectory: _dailySkillDir,
        scriptPath: 'scripts/generate_daily_briefing_a2ui.py',
        args: [
          '--source',
          'compose-live',
          '--schedule-source',
          'ics-url',
          '--ics-url',
          _scheduleFeedUrl,
          '--schedule-days',
          '120',
          '--commute-source',
          'mock',
        ],
      );
      return;
    case '/sports-briefing':
      await _proxyNormalizedSkill(
        request.response,
        workingDirectory: _sportsSkillDir,
        scriptPath: 'scripts/generate_sports_a2ui.py',
        args: const ['--source', 'thesportsdb', '--league', 'kleague1'],
      );
      return;
    case '/finance-briefing':
      await _proxyNormalizedSkill(
        request.response,
        workingDirectory: _financeSkillDir,
        scriptPath: 'scripts/generate_finance_a2ui.py',
        args: ['--source', 'naver-public', '--watchlist', _financeWatchlist],
      );
      return;
    default:
      await _writeNotFound(request.response);
      return;
  }
}

Future<void> _proxyUpstreamText(
  HttpResponse response, {
  required String url,
  required String contentType,
}) async {
  final result = await Process.run('curl', [
    '-L',
    '--fail',
    '--silent',
    '--show-error',
    url,
  ]);

  if (result.exitCode != 0) {
    await _writeJsonError(
      response,
      statusCode: HttpStatus.badGateway,
      error: 'upstream_fetch_failed',
      message: _trimMessage(
        result.stderr,
        fallback: 'Failed to fetch upstream feed.',
      ),
    );
    return;
  }

  response.statusCode = HttpStatus.ok;
  response.headers.set(HttpHeaders.contentTypeHeader, contentType);
  response.write(result.stdout);
  await response.close();
}

Future<void> _proxyNormalizedSkill(
  HttpResponse response, {
  required String workingDirectory,
  required String scriptPath,
  required List<String> args,
}) async {
  final tempDirectory = await Directory.systemTemp.createTemp(
    'openclaw-tv-proxy-',
  );
  final normalizedOutput = File('${tempDirectory.path}/normalized.json');

  try {
    final result = await Process.run('python3', [
      scriptPath,
      ...args,
      '--dump-normalized',
      normalizedOutput.path,
    ], workingDirectory: workingDirectory);

    if (result.exitCode != 0) {
      await _writeJsonError(
        response,
        statusCode: HttpStatus.badGateway,
        error: 'skill_process_failed',
        message: _trimMessage(
          result.stderr,
          fallback: 'Skill script failed to generate normalized data.',
        ),
      );
      return;
    }

    if (!await normalizedOutput.exists()) {
      await _writeJsonError(
        response,
        statusCode: HttpStatus.badGateway,
        error: 'normalized_output_missing',
        message: 'Skill script completed but no normalized JSON was written.',
      );
      return;
    }

    response.statusCode = HttpStatus.ok;
    response.headers.contentType = ContentType.json;
    response.write(await normalizedOutput.readAsString());
    await response.close();
  } finally {
    if (await tempDirectory.exists()) {
      await tempDirectory.delete(recursive: true);
    }
  }
}

Future<void> _writeNotFound(HttpResponse response) async {
  await _writeJsonError(
    response,
    statusCode: HttpStatus.notFound,
    error: 'not_found',
    message:
        'Use GET /yonhap-feed, /schedule-feed, /daily-briefing, /sports-briefing, or /finance-briefing',
  );
}

Future<void> _writeJsonError(
  HttpResponse response, {
  required int statusCode,
  required String error,
  required String message,
}) async {
  response.statusCode = statusCode;
  response.headers.contentType = ContentType.json;
  response.write(jsonEncode({'error': error, 'message': message}));
  await response.close();
}

String _trimMessage(Object? value, {required String fallback}) {
  final text = '$value'.trim();
  if (text.isEmpty) {
    return fallback;
  }
  return text;
}

void _addCorsHeaders(HttpResponse response) {
  response.headers.set(HttpHeaders.accessControlAllowOriginHeader, '*');
  response.headers.set(
    HttpHeaders.accessControlAllowMethodsHeader,
    'GET, OPTIONS',
  );
  response.headers.set(
    HttpHeaders.accessControlAllowHeadersHeader,
    'Content-Type',
  );
}
