#!/usr/bin/env python3
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / '_shared'))

from scenario_a2ui import ScenarioSpec, main


SPEC = ScenarioSpec(
    skill_name='tv-media-companion',
    title='TV Media Companion',
    default_input=Path(__file__).resolve().parents[1] / 'references' / 'mock_surface.json',
    default_surface_id='media_companion_main',
    loading_title='작품 정보 준비 중',
    loading_detail='현재 재생 중인 작품의 출연진과 가이드를 정리하고 있습니다.',
    loading_hint='작품 제목과 현재 에피소드 정보를 먼저 채웁니다.',
    empty_title='표시할 작품 정보 없음',
    empty_detail='현재 재생 중인 콘텐츠 정보가 없습니다.',
    empty_hint='playback context 또는 mock 메타데이터를 먼저 연결해 주세요.',
    error_title='작품 정보를 불러오지 못했습니다',
    error_detail='재생 컨텍스트 또는 메타데이터 소스를 확인해 주세요.',
    error_hint='스포일러를 피하고 권리 문제가 있는 자산은 노출하지 않는 편이 안전합니다.',
    retry_event='refreshMediaCompanion',
)


if __name__ == '__main__':
    raise SystemExit(main(SPEC))
