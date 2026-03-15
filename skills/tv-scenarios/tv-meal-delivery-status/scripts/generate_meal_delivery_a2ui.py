#!/usr/bin/env python3
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / '_shared'))

from scenario_a2ui import ScenarioSpec, main


SPEC = ScenarioSpec(
    skill_name='tv-meal-delivery-status',
    title='TV Meal Delivery Status',
    default_input=Path(__file__).resolve().parents[1] / 'references' / 'mock_surface.json',
    default_surface_id='delivery_main',
    loading_title='주문 상태 준비 중',
    loading_detail='현재 주문 진행 상태와 ETA를 확인하고 있습니다.',
    loading_hint='남은 시간과 현재 단계부터 먼저 표시합니다.',
    empty_title='표시할 주문 없음',
    empty_detail='최근 연결된 주문 상태가 아직 없습니다.',
    empty_hint='mock 주문 상태 또는 파트너 연동을 먼저 확인해 주세요.',
    error_title='주문 상태를 불러오지 못했습니다',
    error_detail='주문 상태 API 또는 계정 연동 상태를 확인해 주세요.',
    error_hint='주소와 연락처는 TV 공용 화면에 그대로 노출하지 않는 편이 안전합니다.',
    retry_event='refreshDelivery',
)


if __name__ == '__main__':
    raise SystemExit(main(SPEC))
