import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import check_docs


class DocsCheckTest(unittest.TestCase):
    def test_markdown_and_html_links_are_extracted(self):
        self.assertEqual(
            check_docs.extract_links("[guide](docs/guide.html) ![map](assets/map.svg)", False),
            ["docs/guide.html", "assets/map.svg"],
        )
        self.assertEqual(
            check_docs.extract_links('<a href="docs/guide.html">x</a><img src="assets/map.svg">', True),
            ["docs/guide.html", "assets/map.svg"],
        )

    def test_external_and_fragment_targets_are_ignored(self):
        self.assertIsNone(check_docs.local_target(ROOT / "README.md", "https://example.com"))
        self.assertIsNone(check_docs.local_target(ROOT / "README.md", "#layout"))
        self.assertIsNone(check_docs.local_target(ROOT / "README.md", "mailto:test@example.com"))
        self.assertIsNone(check_docs.local_target(ROOT / "docs" / "story-sketchbook.html", "'+hash+'"))

    def test_relative_target_resolves_from_source_directory(self):
        target = check_docs.local_target(ROOT / "docs" / "README.md", "platform.md")
        self.assertEqual(target, (ROOT / "docs" / "platform.md").resolve())

    def test_current_repository_links_exist(self):
        self.assertEqual(check_docs.check_links(check_docs.docs_files()), [])

    def test_active_documents_reject_retired_selectors(self):
        self.assertEqual(check_docs.check_stale(sorted(set(check_docs.ACTIVE_STALE_SCAN))), [])


if __name__ == "__main__":
    unittest.main()
