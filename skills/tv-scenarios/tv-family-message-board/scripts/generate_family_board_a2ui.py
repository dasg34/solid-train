#!/usr/bin/env python3
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / '_shared'))

from scenario_a2ui import ScenarioSpec, main


SPEC = ScenarioSpec(
    skill_name='tv-family-message-board',
    title='TV Family Message Board',
    default_input=Path(__file__).resolve().parents[1] / 'references' / 'mock_surface.json',
    default_surface_id='family_board_main',
    loading_title='가족 보드 준비 중',
    loading_detail='오늘 일정과 공지, 집안 메모를 정리하고 있습니다.',
    loading_hint='학교 공지와 오늘 해야 할 일을 먼저 채웁니다.',
    empty_title='표시할 가족 보드 없음',
    empty_detail='공유된 알림과 일정이 아직 없습니다.',
    empty_hint='가족용 mock 데이터나 공유 캘린더를 먼저 연결해 주세요.',
    error_title='가족 보드를 불러오지 못했습니다',
    error_detail='공유 데이터 소스 또는 권한 모델 상태를 확인해 주세요.',
    error_hint='미성년자 정보와 개인 메시지는 요약형으로만 노출하는 편이 안전합니다.',
    retry_event='refreshFamilyBoard',
)


if __name__ == '__main__':
    raise SystemExit(main(SPEC))
