import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import gen_making_of


class MakingOfTest(unittest.TestCase):
    def test_headings_paragraphs_and_inline_markup(self):
        page = gen_making_of.render(
            "# Title\n\nA **bold** line with `code` and a [link](../music/README.md).\n"
        )
        self.assertNotIn("<h1 id=", page)  # the page header owns the title
        self.assertIn("Generated from", page)
        self.assertIn("<strong>bold</strong>", page)
        self.assertIn("<code>code</code>", page)
        self.assertIn(
            '<a href="https://github.com/FreezingSnail/sub-hauler/blob/main/music/README.md">link</a>',
            page,
        )

    def test_code_fences_escape_html(self):
        page = gen_making_of.render("```sh\nmake build > log.txt && true\n```\n")
        self.assertIn("<pre><code>make build &gt; log.txt &amp;&amp; true</code></pre>", page)

    def test_table_alignment_and_cells(self):
        page = gen_making_of.render(
            "| Name | Limit |\n| --- | ---: |\n| Flash | `29,696 B` |\n"
        )
        self.assertIn('<th class="align-right">Limit</th>', page)
        self.assertIn('<td class="align-right"><code>29,696 B</code></td>', page)
        self.assertIn('<div class="table-wrap"><table>', page)

    def test_lists_keep_wrapped_lines(self):
        page = gen_making_of.render("- first line\n  continues here;\n- second\n")
        self.assertIn("<li>first line continues here;</li>", page)
        self.assertIn("<li>second</li>", page)
        self.assertNotIn("<p>continues here", page)

    def test_directory_links_point_at_github_tree(self):
        self.assertEqual(
            gen_making_of.rewrite_target("../music/", gen_making_of.SOURCE.parent),
            "https://github.com/FreezingSnail/sub-hauler/tree/main/music",
        )
        self.assertEqual(
            gen_making_of.rewrite_target("https://example.com", gen_making_of.SOURCE.parent),
            "https://example.com",
        )

    def test_checked_in_page_matches_markdown_source(self):
        self.assertTrue(gen_making_of.OUTPUT.exists(), "run `make docs-page`")
        self.assertEqual(
            gen_making_of.OUTPUT.read_text(encoding="utf-8"),
            gen_making_of.generate(),
        )


if __name__ == "__main__":
    unittest.main()
