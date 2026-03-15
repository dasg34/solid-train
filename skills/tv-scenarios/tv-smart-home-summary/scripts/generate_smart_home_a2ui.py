#!/usr/bin/env python3
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / '_shared'))

from scenario_a2ui import ScenarioSpec, main


SPEC = ScenarioSpec(
    skill_name='tv-smart-home-summary',
    title='TV Smart Home Summary',
    default_input=Path(__file__).resolve().parents[1] / 'references' / 'mock_surface.json',
    default_surface_id='smart_home_main',
    loading_title='집 상태 준비 중',
    loading_detail='문, 조명, 실내 환경 상태를 모으고 있습니다.',
    loading_hint='이상 상태와 안전 관련 항목을 먼저 채웁니다.',
    empty_title='표시할 집 상태 없음',
    empty_detail='연결된 홈 디바이스 요약이 아직 없습니다.',
    empty_hint='홈 허브 연동 또는 mock 디바이스 상태를 먼저 확인해 주세요.',
    error_title='집 상태를 불러오지 못했습니다',
    error_detail='홈 플랫폼 연결 또는 디바이스 상태 정규화를 확인해 주세요.',
    error_hint='읽기 전용 요약과 실제 제어 경로는 분리하는 편이 안전합니다.',
    retry_event='refreshSmartHome',
)


if __name__ == '__main__':
    raise SystemExit(main(SPEC))
