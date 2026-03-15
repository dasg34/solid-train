#!/usr/bin/env python3
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / '_shared'))

from scenario_a2ui import ScenarioSpec, main


SPEC = ScenarioSpec(
    skill_name='tv-wellness-card',
    title='TV Wellness Card',
    default_input=Path(__file__).resolve().parents[1] / 'references' / 'mock_surface.json',
    default_surface_id='wellness_main',
    loading_title='웰니스 카드 준비 중',
    loading_detail='활동량과 수면 요약을 정리하고 있습니다.',
    loading_hint='오늘 가장 가벼운 다음 행동부터 먼저 채웁니다.',
    empty_title='표시할 웰니스 카드 없음',
    empty_detail='연결된 건강 요약 카드가 아직 없습니다.',
    empty_hint='mock 건강 데이터나 동기화 상태를 먼저 확인해 주세요.',
    error_title='웰니스 카드를 불러오지 못했습니다',
    error_detail='건강 데이터 소스 또는 권한 상태를 확인해 주세요.',
    error_hint='민감한 건강 지표는 TV 공용 화면에 과도하게 노출하지 않는 편이 안전합니다.',
    retry_event='refreshWellness',
)


if __name__ == '__main__':
    raise SystemExit(main(SPEC))
