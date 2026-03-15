#!/usr/bin/env python3
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / '_shared'))

from scenario_a2ui import ScenarioSpec, main


SPEC = ScenarioSpec(
    skill_name='tv-shopping-decision-support',
    title='TV Shopping Decision Support',
    default_input=Path(__file__).resolve().parents[1] / 'references' / 'mock_surface.json',
    default_surface_id='shopping_main',
    loading_title='상품 비교 준비 중',
    loading_detail='핵심 비교 포인트와 추천 액션을 정리하고 있습니다.',
    loading_hint='비교 대상과 가장 큰 차이점부터 먼저 채웁니다.',
    empty_title='표시할 상품 비교 없음',
    empty_detail='현재 비교할 상품 카드가 없습니다.',
    empty_hint='mock 상품 비교 데이터나 카탈로그 연결을 먼저 확인해 주세요.',
    error_title='상품 비교를 불러오지 못했습니다',
    error_detail='상품 카탈로그 또는 가격 데이터 상태를 확인해 주세요.',
    error_hint='오래된 가격과 과장된 추천 문구는 피하고 근거를 함께 보여주는 편이 좋습니다.',
    retry_event='refreshShoppingDecision',
)


if __name__ == '__main__':
    raise SystemExit(main(SPEC))
