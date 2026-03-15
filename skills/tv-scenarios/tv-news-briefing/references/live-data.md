# Live Data Notes

- Verified live Yonhap News TV RSS access on March 15, 2026 using:
  `https://www.yonhapnewstv.co.kr/browse/feed/`
- The skill can emit A2UI directly from the live RSS feed with:
  `python3 scripts/generate_news_a2ui.py --source yonhap-rss --count 6`
- Keep live rendering headline-first. Avoid pulling long article body text into the TV summary by default.
- Preserve publisher attribution and last update time whenever live feed items are shown.
