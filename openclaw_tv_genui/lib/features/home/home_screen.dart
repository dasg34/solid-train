import 'package:flutter/material.dart';

import '../../core/a2ui/a2ui_payload_source.dart';
import 'models/scenario_entry.dart';
import 'widgets/genui_scenario_surface.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({required this.payloadSource, super.key});

  final A2uiPayloadSource payloadSource;

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  late ScenarioEntry _selectedScenario;

  @override
  void initState() {
    super.initState();
    _selectedScenario = scenarioCatalog.first;
  }

  @override
  Widget build(BuildContext context) {
    final isWideLayout = MediaQuery.sizeOf(context).width >= 720;

    return Scaffold(
      body: DecoratedBox(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            colors: [Color(0xFF07131C), Color(0xFF0D202C), Color(0xFF12302C)],
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
          ),
        ),
        child: SafeArea(
          child: Padding(
            padding: const EdgeInsets.all(32),
            child: isWideLayout
                ? Row(
                    children: [
                      SizedBox(
                        width: 400,
                        child: _ScenarioRail(
                          selectedScenario: _selectedScenario,
                          onSelectScenario: _handleScenarioChanged,
                        ),
                      ),
                      const SizedBox(width: 32),
                      Expanded(
                        child: _PreviewPanel(
                          scenario: _selectedScenario,
                          payloadSource: widget.payloadSource,
                        ),
                      ),
                    ],
                  )
                : Column(
                    children: [
                      SizedBox(
                        height: 330,
                        child: _ScenarioRail(
                          selectedScenario: _selectedScenario,
                          onSelectScenario: _handleScenarioChanged,
                        ),
                      ),
                      const SizedBox(height: 24),
                      Expanded(
                        child: _PreviewPanel(
                          scenario: _selectedScenario,
                          payloadSource: widget.payloadSource,
                        ),
                      ),
                    ],
                  ),
          ),
        ),
      ),
    );
  }

  void _handleScenarioChanged(ScenarioEntry scenario) {
    setState(() {
      _selectedScenario = scenario;
    });
  }
}

class _ScenarioRail extends StatelessWidget {
  const _ScenarioRail({
    required this.selectedScenario,
    required this.onSelectScenario,
  });

  final ScenarioEntry selectedScenario;
  final ValueChanged<ScenarioEntry> onSelectScenario;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);

    return DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.05),
        borderRadius: BorderRadius.circular(32),
        border: Border.all(color: Colors.white.withValues(alpha: 0.12)),
      ),
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('OpenClaw TV', style: theme.textTheme.headlineSmall),
            const SizedBox(height: 8),
            Text(
              'Samsung Tizen TV용 GenUI PoC',
              style: theme.textTheme.titleMedium?.copyWith(
                color: Colors.white.withValues(alpha: 0.76),
              ),
            ),
            const SizedBox(height: 16),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: const [
                Chip(label: Text('한국 기준')),
                Chip(label: Text('ko-KR / Asia-Seoul')),
                Chip(label: Text('날씨·뉴스·일정·스포츠·금융 live data')),
                Chip(label: Text('daily briefing orchestrator')),
                Chip(label: Text('전체 skill template preview')),
              ],
            ),
            const SizedBox(height: 24),
            Text('준비된 템플릿', style: theme.textTheme.titleMedium),
            const SizedBox(height: 12),
            Expanded(
              child: ListView.separated(
                itemCount: scenarioCatalog.length,
                separatorBuilder: (_, __) => const SizedBox(height: 12),
                itemBuilder: (context, index) {
                  final scenario = scenarioCatalog[index];
                  final isSelected = scenario.id == selectedScenario.id;

                  return InkWell(
                    onTap: () => onSelectScenario(scenario),
                    borderRadius: BorderRadius.circular(24),
                    child: AnimatedContainer(
                      duration: const Duration(milliseconds: 220),
                      curve: Curves.easeOutCubic,
                      padding: const EdgeInsets.all(18),
                      decoration: BoxDecoration(
                        color: isSelected
                            ? theme.colorScheme.primary.withValues(alpha: 0.18)
                            : Colors.white.withValues(alpha: 0.04),
                        borderRadius: BorderRadius.circular(24),
                        border: Border.all(
                          color: isSelected
                              ? theme.colorScheme.primary.withValues(
                                  alpha: 0.75,
                                )
                              : Colors.white.withValues(alpha: 0.10),
                        ),
                      ),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            scenario.title,
                            style: theme.textTheme.titleMedium,
                          ),
                          const SizedBox(height: 6),
                          Text(
                            scenario.summary,
                            style: theme.textTheme.bodyMedium?.copyWith(
                              color: theme.colorScheme.secondary,
                            ),
                          ),
                          const SizedBox(height: 14),
                          Chip(label: Text(scenario.pattern.label)),
                        ],
                      ),
                    ),
                  );
                },
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _PreviewPanel extends StatelessWidget {
  const _PreviewPanel({required this.scenario, required this.payloadSource});

  final ScenarioEntry scenario;
  final A2uiPayloadSource payloadSource;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);

    return DecoratedBox(
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.05),
        borderRadius: BorderRadius.circular(32),
        border: Border.all(color: Colors.white.withValues(alpha: 0.12)),
      ),
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Wrap(
              spacing: 10,
              runSpacing: 10,
              crossAxisAlignment: WrapCrossAlignment.center,
              children: [
                Container(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 14,
                    vertical: 8,
                  ),
                  decoration: BoxDecoration(
                    color: Colors.white.withValues(alpha: 0.08),
                    borderRadius: BorderRadius.circular(999),
                  ),
                  child: Text(
                    '선택된 템플릿: ${scenario.title}',
                    style: theme.textTheme.labelLarge,
                  ),
                ),
                Chip(label: Text(scenario.pattern.label)),
                Chip(label: Text('유연한 비율 surface')),
              ],
            ),
            const SizedBox(height: 12),
            Text(
              '${scenario.pattern.detail}\n"${scenario.summary}"',
              style: theme.textTheme.bodyLarge?.copyWith(
                color: Colors.white.withValues(alpha: 0.74),
              ),
            ),
            const SizedBox(height: 20),
            Expanded(
              child: LayoutBuilder(
                builder: (context, constraints) {
                  final frame = _frameForPattern(scenario.pattern);

                  return DecoratedBox(
                    decoration: BoxDecoration(
                      borderRadius: BorderRadius.circular(28),
                      gradient: const LinearGradient(
                        colors: [
                          Color(0xFF091521),
                          Color(0xFF101E29),
                          Color(0xFF0C1821),
                        ],
                        begin: Alignment.topLeft,
                        end: Alignment.bottomRight,
                      ),
                    ),
                    child: Stack(
                      children: [
                        Positioned(
                          top: 18,
                          left: 18,
                          child: Container(
                            padding: const EdgeInsets.symmetric(
                              horizontal: 12,
                              vertical: 6,
                            ),
                            decoration: BoxDecoration(
                              color: Colors.white.withValues(alpha: 0.08),
                              borderRadius: BorderRadius.circular(999),
                            ),
                            child: Text(
                              'TV canvas · flexible ratio preview',
                              style: theme.textTheme.labelMedium,
                            ),
                          ),
                        ),
                        Positioned(
                          right: 70,
                          top: 80,
                          child: _GlowOrb(
                            size: 180,
                            color: theme.colorScheme.secondary,
                          ),
                        ),
                        Positioned(
                          left: 60,
                          bottom: 50,
                          child: _GlowOrb(
                            size: 240,
                            color: theme.colorScheme.primary,
                          ),
                        ),
                        Align(
                          alignment: frame.alignment,
                          child: Padding(
                            padding: const EdgeInsets.all(28),
                            child: AnimatedSwitcher(
                              duration: const Duration(milliseconds: 280),
                              switchInCurve: Curves.easeOutCubic,
                              switchOutCurve: Curves.easeInCubic,
                              child: SizedBox(
                                key: ValueKey(scenario.id),
                                width: constraints.maxWidth * frame.widthFactor,
                                height:
                                    constraints.maxHeight * frame.heightFactor,
                                child: DecoratedBox(
                                  decoration: BoxDecoration(
                                    borderRadius: BorderRadius.circular(34),
                                    boxShadow: [
                                      BoxShadow(
                                        color: Colors.black.withValues(
                                          alpha: 0.28,
                                        ),
                                        blurRadius: 30,
                                        offset: const Offset(0, 18),
                                      ),
                                    ],
                                  ),
                                  child: ClipRRect(
                                    borderRadius: BorderRadius.circular(34),
                                    child: GenUiScenarioSurface(
                                      scenario: scenario,
                                      payloadSource: payloadSource,
                                    ),
                                  ),
                                ),
                              ),
                            ),
                          ),
                        ),
                      ],
                    ),
                  );
                },
              ),
            ),
          ],
        ),
      ),
    );
  }

  _SurfaceFrame _frameForPattern(TvSurfacePattern pattern) {
    return switch (pattern) {
      TvSurfacePattern.immersive => const _SurfaceFrame(
        alignment: Alignment.center,
        widthFactor: 0.84,
        heightFactor: 0.82,
      ),
      TvSurfacePattern.sidePanel => const _SurfaceFrame(
        alignment: Alignment.centerRight,
        widthFactor: 0.42,
        heightFactor: 0.80,
      ),
      TvSurfacePattern.centerCard => const _SurfaceFrame(
        alignment: Alignment.center,
        widthFactor: 0.58,
        heightFactor: 0.60,
      ),
    };
  }
}

class _GlowOrb extends StatelessWidget {
  const _GlowOrb({required this.size, required this.color});

  final double size;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return IgnorePointer(
      child: Container(
        width: size,
        height: size,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          gradient: RadialGradient(
            colors: [
              color.withValues(alpha: 0.28),
              color.withValues(alpha: 0.08),
              Colors.transparent,
            ],
          ),
        ),
      ),
    );
  }
}

class _SurfaceFrame {
  const _SurfaceFrame({
    required this.alignment,
    required this.widthFactor,
    required this.heightFactor,
  });

  final Alignment alignment;
  final double widthFactor;
  final double heightFactor;
}
