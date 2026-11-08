#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Parse SketchCAD用户手册.html into intermediate blocks, emit DOCX + PDF.
Single parser, two emitters -> consistent layout across Word and PDF."""
import re, os, sys
from html.parser import HTMLParser
from html import unescape

BASE = r"D:\01-CAD\02-avticationCAD\sketch-cad-cpp\docs\user-manual"
HTML_PATH = os.path.join(BASE, "SketchCAD用户手册.html")
IMG_DIR = os.path.join(BASE, "img")

CJK_RE = re.compile(r'[\u3000-\u303f\u4e00-\u9fff\uff00-\uffef]')
def is_cjk(c): return bool(c) and bool(CJK_RE.match(c))

# ---------------- token helpers ----------------
# token: ('t', text, bold, code) or ('br',)

def flush_runs(toks):
    """Collapse whitespace, drop spaces between CJK chars, return runs list.
    run: ('br',) or (text, bold, code)"""
    runs = []
    prev = [None]  # last emitted char
    def emit(text, bold, code):
        if text == "": return
        if runs and runs[-1] != ('br',) and runs[-1][1] == bold and runs[-1][2] == code:
            runs[-1] = (runs[-1][0] + text, bold, code)
        else:
            runs.append((text, bold, code))
        prev[0] = text[-1]
    i = 0
    while i < len(toks):
        tok = toks[i]
        if tok[0] == 'br':
            runs.append(('br',)); prev[0] = None; i += 1; continue
        _, text, bold, code = tok
        text = re.sub(r'\s+', ' ', text)
        if text == '': i += 1; continue
        m = re.match(r'^( +)', text)
        if m:
            rest = text[m.end():]
            first = rest[0] if rest else None
            if prev[0] is not None and not (is_cjk(prev[0]) and first and is_cjk(first)):
                emit(' ', bold, code)
            text = rest
            if not text: i += 1; continue
        m = re.search(r'( +)$', text)
        trail = ''
        if m:
            trail = m.group(1); text = text[:m.start()]
        if text: emit(text, bold, code)
        if trail:
            nxt = None; j = i + 1
            while j < len(toks):
                if toks[j][0] == 'br': break
                c = re.sub(r'\s+', ' ', toks[j][1]).lstrip()
                if c: nxt = c[0]; break
                j += 1
            if prev[0] is not None and nxt is not None and not (is_cjk(prev[0]) and is_cjk(nxt)):
                emit(' ', bold, code)
        i += 1
    while runs and runs[0] == ('br',): runs.pop(0)
    while runs and runs[-1] == ('br',): runs.pop()
    if runs and runs[0][0] == ' ': runs[0] = (runs[0][0].lstrip(),) + runs[0][1:]
    if runs and runs[-1] != ('br',) and runs[-1][0].endswith(' '):
        runs[-1] = (runs[-1][0].rstrip(),) + runs[-1][1:]
    return runs

def runs_text(runs):
    return ''.join(r[0] for r in runs if r[0] != 'br' and r[0] != ' ')

class Block:
    def __init__(self, bkind, **kw):
        self.kind = bkind
        self.__dict__.update(kw)

# ---------------- parser ----------------
class ManualParser(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.blocks = []
        self.mode = None          # None|'cover'|'toc'|'fig'|'callout'|'table'
        self.skip_depth = 0       # inside <style>/<script>
        self.bold = 0
        self.code = 0
        self.kbd = 0
        # inline collection target: list appended to
        self.toks = None
        self.target = None        # (kind, Block-or-dict)
        # fig state
        self.fig = None
        self.in_row = False
        self.row_imgs = None
        self.svg_count = 0
        # table state
        self.table_rows = None
        self.cell_toks = None
        self.cell_tag = None
        # list state
        self.list_kind = None     # 'ul'|'ol'
        self.list_items = None
        # toc state
        self.toc_depth = 0
        self.toc_entries = None
        self.toc_cur = None
        # cover state
        self.cover = None
        self.cover_target = None

    # ---- inline helpers ----
    def start_inline(self, target, owner):
        self.toks = []
        self.target = (target, owner)
    def drop_inline(self):
        self.toks = None; self.target = None
    def add_t(self, s):
        if self.toks is not None and s:
            self.toks.append(('t', s, self.bold > 0, self.code > 0))

    def cls(self, attrs):
        return (attrs.get('class') or '').split()

    def handle_starttag(self, tag, attrs):
        ad = dict(attrs)
        cl = self.cls(ad)
        if tag in ('style', 'script'):
            self.skip_depth += 1; return
        if self.skip_depth: return
        if tag == 'b' or tag == 'strong': self.bold += 1; return
        if tag == 'code': self.code += 1; return
        if tag == 'span' and 'kbd' in cl:
            self.kbd += 1; self.code += 1; return
        if tag == 'br':
            if self.toks is not None: self.toks.append(('br',))
            return
        if tag == 'hr': return
        if tag == 'img':
            src = ad.get('src', '')
            if self.mode == 'fig' and self.fig is not None:
                if self.in_row: self.row_imgs.append(src)
                else: self.fig['imgs'].append(src)
            return
        if tag == 'svg':
            idx = self.svg_count; self.svg_count += 1
            if self.mode == 'fig' and self.fig is not None:
                self.fig['svg'] = idx
            return

        if tag == 'section' and 'cover' in cl:
            self.mode = 'cover'
            self.cover = {'badge': [], 'title': [], 'sub': [], 'meta': [], 'intro': []}
            return
        if tag == 'div':
            if 'toc' in cl:
                self.mode = 'toc'; self.toc_entries = []; self.toc_depth = 0
                return
            if 'fig' in cl:
                self.mode = 'fig'
                self.fig = {'imgs': [], 'svg': None, 'cap': [], 'row': None}
                return
            if 'rowimg' in cl and self.mode == 'fig':
                self.in_row = True; self.row_imgs = []
                return
            if any(c in ('callout', 'tip', 'warn') for c in cl) and 'callout' in cl:
                kind = 'warn' if 'warn' in cl else 'tip'
                self.mode = 'callout'
                self.start_inline('callout', {'kind': kind})
                return
            if 'cap' in cl and self.mode == 'fig':
                self.start_inline('cap', self.fig)
                return
            if 'sub' in cl and self.mode == 'cover':
                self.cover_target = 'sub'; self.toks = []
                return
            if 'meta' in cl and self.mode == 'cover':
                self.cover_target = 'meta'; self.toks = []
                return
            return
        if tag == 'span':
            if 'badge' in cl and self.mode == 'cover':
                self.cover_target = 'badge'; self.toks = []
            return
        if tag in ('h1',):
            if self.mode == 'cover':
                self.cover_target = 'title'; self.toks = []
            return
        if tag == 'h2':
            if self.mode == 'toc':
                self.start_inline('toc_title', None); return
            if 'chap' in cl:
                b = Block('h1'); self.blocks.append(b); self.start_inline('h1', b)
            return
        if tag == 'h3':
            b = Block('h2'); self.blocks.append(b); self.start_inline('h2', b)
            return
        if tag == 'h4':
            b = Block('h3'); self.blocks.append(b); self.start_inline('h3', b)
            return
        if tag == 'p':
            if self.mode == 'cover':
                self.cover_target = 'intro'; self.toks = []
                return
            if 'foot' in cl:
                b = Block('foot'); self.blocks.append(b); self.start_inline('foot', b)
                return
            b = Block('p'); self.blocks.append(b); self.start_inline('p', b)
            return
        if tag in ('ul', 'ol'):
            self.list_kind = 'ul' if tag == 'ul' else 'ol'
            self.list_items = []
            return
        if tag == 'li':
            if self.mode == 'toc':
                self.toc_cur = (self.toc_depth, [])
                self.toks = self.toc_cur[1]
                return
            if self.list_kind:
                item = []
                self.toks = item
                self._cur_item = item
            return
        if tag == 'table':
            self.mode = 'table'; self.table_rows = []
            return
        if tag == 'tr':
            self.table_rows.append([])
            return
        if tag in ('td', 'th'):
            self.cell_tag = tag
            self.cell_toks = []
            self.toks = self.cell_toks
            return
        if tag == 'a':
            if self.mode == 'toc' and self.toc_cur is not None:
                pass
            return
        # ignore everything else

    def handle_startendtag(self, tag, attrs):
        if tag == 'br':
            self.handle_starttag('br', {}); self.handle_endtag('br')
        elif tag == 'img':
            self.handle_starttag('img', attrs)
        elif tag == 'hr':
            pass

    def handle_data(self, data):
        if self.skip_depth: return
        self.add_t(data)

    def _flush_inline(self):
        if self.toks is None or self.target is None:
            self.toks = None; self.target = None; return
        target, owner = self.target
        runs = flush_runs(self.toks)
        if target == 'h1': owner.runs = runs
        elif target == 'h2': owner.runs = runs
        elif target == 'h3': owner.runs = runs
        elif target == 'p': owner.runs = runs
        elif target == 'foot': owner.runs = runs
        elif target == 'cap': owner['cap'] = runs
        elif target == 'callout':
            self.blocks.append(Block('callout', ckind=owner['kind'], runs=runs))
        elif target == 'toc_title': pass
        self.toks = None; self.target = None

    def handle_endtag(self, tag):
        if tag in ('style', 'script'):
            self.skip_depth = max(0, self.skip_depth - 1); return
        if self.skip_depth: return
        if tag in ('b', 'strong'):
            self.bold = max(0, self.bold - 1); return
        if tag == 'code':
            self.code = max(0, self.code - 1); return
        if tag == 'span':
            if self.kbd > 0:
                self.kbd -= 1; self.code = max(0, self.code - 1)
            self._maybe_close_span(); return
        if tag in ('h1', 'h2', 'h3', 'h4', 'p'):
            if self.mode == 'cover' and self.cover_target:
                self.cover[self.cover_target] = flush_runs(self.toks or [])
                self.toks = None; self.cover_target = None
                return
            self._flush_inline(); return
        if tag == 'li':
            if self.mode == 'toc' and self.toc_cur is not None:
                depth, toks = self.toc_cur
                self.toc_entries.append((depth, flush_runs(toks)))
                self.toc_cur = None; self.toks = None
                return
            if self.list_kind and self._cur_item is not None:
                self.list_items.append(flush_runs(self._cur_item))
                self._cur_item = None; self.toks = None
            return
        if tag in ('ul', 'ol'):
            if self.list_kind:
                self.blocks.append(Block('list', lkind=self.list_kind, items=self.list_items))
                self.list_kind = None; self.list_items = None
            return
        if tag in ('td', 'th'):
            if self.cell_toks is not None and self.table_rows:
                self.table_rows[-1].append((self.cell_tag, flush_runs(self.cell_toks)))
            self.cell_toks = None; self.cell_tag = None; self.toks = None
            return
        if tag == 'table':
            if self.mode == 'table' and self.table_rows:
                self.blocks.append(Block('table', rows=self.table_rows))
            self.mode = None; self.table_rows = None
            return
        if tag == 'div':
            # closing the caption div (nested inside fig): flush cap, keep fig open
            if self.target and self.target[0] == 'cap':
                owner = self.target[1]
                owner['cap'] = flush_runs(self.toks)
                self.drop_inline()
                return
            self._end_div(); return
        if tag == 'section':
            self._end_cover(); return

    def _maybe_close_span(self):
        # close badge span collection
        if self.mode == 'cover' and self.cover_target == 'badge' and self.toks is not None:
            self.cover['badge'] = flush_runs(self.toks)
            self.toks = None; self.cover_target = None

    def _end_div(self):
        if self.mode == 'toc':
            self.mode = None
            if self.toc_entries is not None:
                self.blocks.append(Block('toc', title='目录', entries=self.toc_entries))
            self.toc_entries = None
            return
        if self.mode == 'fig':
            if self.in_row:
                # closing the rowimg wrapper: stash row images, outer fig div closes later
                if self.row_imgs and self.fig is not None:
                    self.fig['row'] = list(self.row_imgs)
                self.in_row = False; self.row_imgs = None
                return
            if self.fig is not None:
                if self.fig['cap'] or self.fig['imgs'] or self.fig['svg'] is not None or self.fig['row']:
                    self.blocks.append(Block('fig', imgs=self.fig['imgs'], svg=self.fig['svg'],
                                             cap=self.fig['cap'], row=self.fig['row']))
                self.fig = None
            self.mode = None
            return
        if self.mode == 'callout':
            self._flush_inline(); self.mode = None
            return
        if self.mode == 'cover':
            if self.cover_target in ('sub', 'meta') and self.toks is not None:
                self.cover[self.cover_target] = flush_runs(self.toks)
                self.toks = None; self.cover_target = None
            return

    def _end_cover(self):
        if self.mode == 'cover':
            if self.toks is not None and self.cover_target:
                self.cover[self.cover_target] = flush_runs(self.toks)
            self.blocks.append(Block('cover', **self.cover))
            self.mode = None; self.cover = None; self.toks = None; self.cover_target = None


def parse_manual():
    with open(HTML_PATH, encoding='utf-8') as f:
        html = f.read()
    p = ManualParser()
    p.feed(html)
    p.close()
    return p.blocks

# resolve image src -> absolute path + svg mapping
def fig_image_paths(block):
    """return list of (path, in_row) for a fig block"""
    out = []
    if block.svg is not None:
        out.append((os.path.join(IMG_DIR, f"_svg_{block.svg}.png"), False))
    for src in block.imgs:
        out.append((os.path.join(BASE, src.replace('/', os.sep)), False))
    if block.row:
        for src in block.row:
            out.append((os.path.join(BASE, src.replace('/', os.sep)), True))
    return out

# ================= DOCX emitter =================
def emit_docx(blocks, out_path):
    from docx import Document
    from docx.shared import Pt, Cm, RGBColor
    from docx.enum.text import WD_ALIGN_PARAGRAPH
    from docx.enum.table import WD_TABLE_ALIGNMENT
    from docx.oxml.ns import qn
    from docx.oxml import OxmlElement
    from PIL import Image as PILImage

    BRAND = '1F3352'; BRAND2 = '2E5AAC'; INK = '1F2733'; MUTED = '5B6B7E'
    CODEINK = '0B3D6D'; CODEBG = 'F6F8FB'; LINE = 'D7DEE8'
    SOFT = 'F4F7FB'; SOFT2 = 'EEF3F9'
    TIPBG = 'E8F5EC'; TIPTX = '1E5B39'; TIPBD = 'BFE6CD'
    WARNBG = 'FDF1E3'; WARNTX = '8A5310'; WARNBD = 'F0D3A6'

    doc = Document()
    # page setup A4
    sec = doc.sections[0]
    sec.page_width = Cm(21.0); sec.page_height = Cm(29.7)
    sec.left_margin = sec.right_margin = Cm(2.0)
    sec.top_margin = sec.bottom_margin = Cm(2.0)
    CONTENT = 17.0  # cm

    def set_font(run, size=10.5, bold=False, color=INK, mono=False):
        name = 'Consolas' if mono else 'Microsoft YaHei'
        run.font.name = name
        run.font.size = Pt(size)
        run.font.bold = bold
        run.font.color.rgb = RGBColor.from_string(color)
        rPr = run._element.get_or_add_rPr()
        rf = rPr.find(qn('w:rFonts'))
        if rf is None:
            rf = OxmlElement('w:rFonts'); rPr.insert(0, rf)
        rf.set(qn('w:ascii'), name); rf.set(qn('w:hAnsi'), name)
        rf.set(qn('w:eastAsia'), 'Microsoft YaHei')
        if mono:
            shd = OxmlElement('w:shd')
            shd.set(qn('w:val'), 'clear'); shd.set(qn('w:fill'), CODEBG)
            rPr.append(shd)

    def add_runs(par, runs, size=10.5, color=INK, bold_color=None):
        for r in runs:
            if r == ('br',):
                par.add_run().add_break(); continue
            text, bold, code = r
            run = par.add_run(text)
            if code:
                set_font(run, size=max(9.0, size - 1.0), bold=bold, color=CODEINK, mono=True)
            else:
                set_font(run, size=size, bold=bold, color=(bold_color if bold and bold_color else color))

    def shade_cell(cell, fill):
        tcPr = cell._tc.get_or_add_tcPr()
        shd = OxmlElement('w:shd')
        shd.set(qn('w:val'), 'clear'); shd.set(qn('w:fill'), fill)
        tcPr.append(shd)

    def table_borders(table, color=LINE, sz='4'):
        tblPr = table._tbl.tblPr
        borders = OxmlElement('w:tblBorders')
        for edge in ('top', 'left', 'bottom', 'right', 'insideH', 'insideV'):
            el = OxmlElement(f'w:{edge}')
            el.set(qn('w:val'), 'single'); el.set(qn('w:sz'), sz)
            el.set(qn('w:color'), color)
            borders.append(el)
        tblPr.append(borders)

    def left_accent(par, color=BRAND2):
        pPr = par._p.get_or_add_pPr()
        pBdr = OxmlElement('w:pBdr')
        left = OxmlElement('w:left')
        left.set(qn('w:val'), 'single'); left.set(qn('w:sz'), '24')
        left.set(qn('w:color'), color); left.set(qn('w:space'), '4')
        pBdr.append(left); pPr.append(pBdr)

    def add_picture_par(width_cm, path, align_center=True):
        par = doc.add_paragraph()
        par.paragraph_format.space_before = Pt(4)
        par.paragraph_format.space_after = Pt(2)
        if align_center: par.alignment = WD_ALIGN_PARAGRAPH.CENTER
        run = par.add_run()
        try:
            run.add_picture(path, width=Cm(width_cm))
        except Exception as e:
            set_font(par.add_run(f'[图片缺失: {os.path.basename(path)}]'), 9, color='A00')
        return par

    def caption_par(runs):
        par = doc.add_paragraph()
        par.paragraph_format.space_after = Pt(12)
        add_runs(par, runs, size=9, color=MUTED, bold_color=BRAND)
        return par

    # ---------- emit blocks ----------
    for bi, b in enumerate(blocks):
        k = b.kind
        if k == 'cover':
            t = doc.add_table(rows=1, cols=1)
            t.alignment = WD_TABLE_ALIGNMENT.CENTER
            t.autofit = False
            t.columns[0].width = Cm(CONTENT)
            cell = t.cell(0, 0)
            cell.width = Cm(CONTENT)
            shade_cell(cell, BRAND)
            # pad cell
            tcPr = cell._tc.get_or_add_tcPr()
            mar = OxmlElement('w:tcMar')
            for side, val in (('top', '800'), ('bottom', '800'), ('left', '500'), ('right', '500')):
                el = OxmlElement(f'w:{side}'); el.set(qn('w:w'), val); el.set(qn('w:type'), 'dxa')
                mar.append(el)
            tcPr.append(mar)
            def cpar(align=WD_ALIGN_PARAGRAPH.LEFT, before=0, after=6):
                par = cell.add_paragraph() if cell.paragraphs[0].runs or len(cell.paragraphs) > 1 or cell.paragraphs[0].text else cell.paragraphs[0]
                par.alignment = align
                par.paragraph_format.space_before = Pt(before)
                par.paragraph_format.space_after = Pt(after)
                return par
            p = cpar(); add_runs(p, b.badge, size=9, color='C9D6EA')
            p = cpar(before=10, after=2)
            run = p.add_run(runs_text(b.title)); set_font(run, 34, True, 'FFFFFF')
            p = cpar(after=16)
            run = p.add_run(runs_text(b.sub)); set_font(run, 17, False, 'DDE6F2')
            # meta: one paragraph per line (split runs at <br/>)
            line = []
            meta_lines = []
            for r in b.meta:
                if r == ('br',):
                    meta_lines.append(line); line = []
                else:
                    line.append(r)
            if line: meta_lines.append(line)
            for ln in meta_lines:
                p = cpar(after=2)
                add_runs(p, ln, size=10, color='C9D6EA')
            p = cpar(before=14)
            add_runs(p, b.intro, size=9.5, color='DDE6F2')
            table_borders(t, color=BRAND)
            continue
        if k == 'toc':
            doc.add_page_break()
            p = doc.add_paragraph(); p.paragraph_format.space_after = Pt(10)
            run = p.add_run(b.title); set_font(run, 16, True, BRAND)
            for depth, runs in b.entries:
                p = doc.add_paragraph()
                p.paragraph_format.left_indent = Cm(0.0 if depth <= 1 else 0.7)
                p.paragraph_format.space_after = Pt(3)
                add_runs(p, runs, size=11 if depth <= 1 else 10,
                         color=BRAND if depth <= 1 else '33475F')
            continue
        if k == 'h1':
            p = doc.add_paragraph()
            p.paragraph_format.page_break_before = True
            p.paragraph_format.space_before = Pt(6)
            p.paragraph_format.space_after = Pt(10)
            add_runs(p, b.runs, size=17, color=BRAND)
            for r in p.runs: r.font.bold = True
            left_accent(p)
            continue
        if k == 'h2':
            p = doc.add_paragraph()
            p.paragraph_format.space_before = Pt(14)
            p.paragraph_format.space_after = Pt(6)
            add_runs(p, b.runs, size=13.5, color='23344D')
            for r in p.runs: r.font.bold = True
            continue
        if k == 'h3':
            p = doc.add_paragraph()
            p.paragraph_format.space_before = Pt(10)
            p.paragraph_format.space_after = Pt(4)
            add_runs(p, b.runs, size=11.5, color='2C3E57')
            for r in p.runs: r.font.bold = True
            continue
        if k == 'p':
            p = doc.add_paragraph()
            p.paragraph_format.space_after = Pt(6)
            add_runs(p, b.runs)
            continue
        if k == 'foot':
            p = doc.add_paragraph()
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            p.paragraph_format.space_before = Pt(18)
            add_runs(p, b.runs, size=9, color='8A94A6')
            continue
        if k == 'list':
            n = 0
            for item in b.items:
                p = doc.add_paragraph()
                p.paragraph_format.left_indent = Cm(0.75)
                if b.lkind == "ol":
                    n += 1
                    p.paragraph_format.first_line_indent = Cm(-0.45)
                    add_runs(p, [(f'{n}. ', False, False)] + item)
                else:
                    p.paragraph_format.first_line_indent = Cm(-0.45)
                    add_runs(p, [('• ', False, False)] + item)
                p.paragraph_format.space_after = Pt(3)
            continue
        if k == 'table':
            rows = b.rows
            if not rows: continue
            ncol = max(len(r) for r in rows)
            t = doc.add_table(rows=len(rows), cols=ncol)
            t.style = 'Table Grid'
            table_borders(t, color=LINE)
            for ri, row in enumerate(rows):
                for ci in range(ncol):
                    cell = t.cell(ri, ci)
                    tag, runs = row[ci] if ci < len(row) else ('td', [])
                    par = cell.paragraphs[0]
                    par.paragraph_format.space_after = Pt(2)
                    if tag == 'th':
                        add_runs(par, runs, size=9.5, color=BRAND)
                        for r in par.runs: r.font.bold = True
                        shade_cell(cell, SOFT2)
                    else:
                        add_runs(par, runs, size=9.5)
                        if ri % 2 == 0: shade_cell(cell, SOFT)
            doc.add_paragraph().paragraph_format.space_after = Pt(0)
            continue
        if k == 'fig':
            paths = fig_image_paths(b)
            fulls = [p_ for p_, ir in paths if not ir]
            rows_ = [p_ for p_, ir in paths if ir]
            if rows_:
                t = doc.add_table(rows=1, cols=len(rows_))
                t.alignment = WD_TABLE_ALIGNMENT.CENTER
                for ci, pth in enumerate(rows_):
                    cell = t.cell(0, ci)
                    cell.width = Cm(CONTENT / len(rows_) - 0.2)
                    par = cell.paragraphs[0]
                    par.alignment = WD_ALIGN_PARAGRAPH.CENTER
                    run = par.add_run()
                    try:
                        run.add_picture(pth, width=Cm(CONTENT / len(rows_) - 0.25))
                    except Exception:
                        set_font(par.add_run(f'[图片缺失: {os.path.basename(pth)}]'), 9, color='A00')
            for pth in fulls:
                add_picture_par(CONTENT - 0.5, pth)
            if b.cap:
                caption_par(b.cap)
            continue
        if k == 'callout':
            bg, tx, bd = (TIPBG, TIPTX, TIPBD) if getattr(b,'ckind',b.kind) == 'tip' else (WARNBG, WARNTX, WARNBD)
            t = doc.add_table(rows=1, cols=1)
            t.autofit = False
            t.columns[0].width = Cm(CONTENT)
            cell = t.cell(0, 0); cell.width = Cm(CONTENT)
            shade_cell(cell, bg)
            table_borders(t, color=bd)
            par = cell.paragraphs[0]
            par.paragraph_format.space_before = Pt(2)
            par.paragraph_format.space_after = Pt(2)
            add_runs(par, b.runs, size=9.5, color=tx)
            # bold the leading header if present (text before first CJK colon)
            continue
        # unknown kinds skipped

    doc.save(out_path)
    return out_path

# ================= PDF emitter =================
def emit_pdf(blocks, out_path):
    from reportlab.lib.pagesizes import A4
    from reportlab.lib.units import cm
    from reportlab.lib import colors
    from reportlab.lib.styles import ParagraphStyle
    from reportlab.lib.enums import TA_LEFT, TA_CENTER
    from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer,
                                    Image as RLImage, Table, TableStyle, PageBreak,
                                    NextPageTemplate, KeepTogether, HRFlowable)
    from reportlab.pdfbase import pdfmetrics
    from reportlab.pdfbase.ttfonts import TTFont
    from reportlab.lib.fonts import addMapping
    from PIL import Image as PILImage

    pdfmetrics.registerFont(TTFont('YaHei', r'C:\Windows\Fonts\msyh.ttc', subfontIndex=0))
    pdfmetrics.registerFont(TTFont('YaHeiB', r'C:\Windows\Fonts\msyhbd.ttc', subfontIndex=0))
    addMapping('YaHei', 0, 0, 'YaHei')
    addMapping('YaHei', 1, 0, 'YaHeiB')
    addMapping('YaHei', 0, 1, 'YaHei')
    addMapping('YaHei', 1, 1, 'YaHeiB')
    pdfmetrics.registerFont(TTFont('Mono', r'C:\Windows\Fonts\consola.ttf'))

    BRAND = colors.HexColor('#1F3352'); BRAND2 = colors.HexColor('#2E5AAC')
    INK = colors.HexColor('#1F2733'); MUTED = colors.HexColor('#5B6B7E')
    CODEINK = colors.HexColor('#0B3D6D'); LINE = colors.HexColor('#D7DEE8')
    SOFT = colors.HexColor('#F4F7FB'); SOFT2 = colors.HexColor('#EEF3F9')
    TIPBG = colors.HexColor('#E8F5EC'); TIPTX = colors.HexColor('#1E5B39'); TIPBD = colors.HexColor('#BFE6CD')
    WARNBG = colors.HexColor('#FDF1E3'); WARNTX = colors.HexColor('#8A5310'); WARNBD = colors.HexColor('#F0D3A6')

    PAGE_W, PAGE_H = A4
    M = 2.0 * cm
    CONTENT = PAGE_W - 2 * M

    def esc(s): return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')

    def runs_markup(runs):
        out = []
        for r in runs:
            if r == ('br',): out.append('<br/>'); continue
            text, bold, code = r
            t = esc(text)
            if code:
                t = f'<font face="Mono" color="#0B3D6D">{t}</font>'
            if bold: t = f'<b>{t}</b>'
            out.append(t)
        return ''.join(out)

    S = {}
    S['body'] = ParagraphStyle('body', fontName='YaHei', fontSize=10, leading=16,
                               textColor=INK, alignment=TA_LEFT, spaceAfter=6, wordWrap='CJK')
    S['h1'] = ParagraphStyle('h1', fontName='YaHeiB', fontSize=17, leading=24,
                             textColor=BRAND, spaceBefore=6, spaceAfter=12, wordWrap='CJK')
    S['h2'] = ParagraphStyle('h2', fontName='YaHeiB', fontSize=13.5, leading=20,
                             textColor=colors.HexColor('#23344D'), spaceBefore=14, spaceAfter=6, wordWrap='CJK')
    S['h3'] = ParagraphStyle('h3', fontName='YaHeiB', fontSize=11.5, leading=17,
                             textColor=colors.HexColor('#2C3E57'), spaceBefore=10, spaceAfter=4, wordWrap='CJK')
    S['cap'] = ParagraphStyle('cap', fontName='YaHei', fontSize=8.5, leading=13,
                              textColor=MUTED, spaceBefore=2, spaceAfter=12, wordWrap='CJK')
    S['cell'] = ParagraphStyle('cell', fontName='YaHei', fontSize=9, leading=13.5,
                               textColor=INK, wordWrap='CJK')
    S['cellh'] = ParagraphStyle('cellh', fontName='YaHeiB', fontSize=9, leading=13.5,
                                textColor=BRAND, wordWrap='CJK')
    S['li'] = ParagraphStyle('li', fontName='YaHei', fontSize=10, leading=16,
                             textColor=INK, leftIndent=16, firstLineIndent=-12,
                             spaceAfter=3, wordWrap='CJK')
    S['foot'] = ParagraphStyle('foot', fontName='YaHei', fontSize=8.5, leading=13,
                               textColor=colors.HexColor('#8A94A6'), alignment=TA_CENTER,
                               spaceBefore=18, wordWrap='CJK')

    def h1_border():
        t = Table([['']], colWidths=[CONTENT], rowHeights=[0.1])
        return None  # not used; accent via table style below

    def fig_flow(b):
        flows = []
        paths = fig_image_paths(b)
        fulls = [p for p, ir in paths if not ir]
        rows_ = [p for p, ir in paths if ir]
        if rows_:
            imgs = []
            for pth in rows_:
                with PILImage.open(pth) as im:
                    w, h = im.size
                tw = (CONTENT - 8) / len(rows_)
                imgs.append(RLImage(pth, width=tw, height=tw * h / w))
            t = Table([imgs], colWidths=[CONTENT / len(rows_)] * len(rows_))
            t.setStyle(TableStyle([
                ('ALIGN', (0, 0), (-1, -1), 'CENTER'),
                ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
                ('LEFTPADDING', (0, 0), (-1, -1), 2),
                ('RIGHTPADDING', (0, 0), (-1, -1), 2),
            ]))
            flows.append(t)
        for pth in fulls:
            with PILImage.open(pth) as im:
                w, h = im.size
            flows.append(RLImage(pth, width=CONTENT - 4, height=(CONTENT - 4) * h / w))
        if b.cap:
            flows.append(Paragraph(runs_markup(b.cap), S['cap']))
        return [KeepTogether(flows)] if flows else []

    def callout_flow(b):
        bg, tx, bd = (TIPBG, TIPTX, TIPBD) if getattr(b,'ckind',b.kind) == 'tip' else (WARNBG, WARNTX, WARNBD)
        st = ParagraphStyle('co', parent=S['body'], textColor=tx, fontSize=9.5, leading=15)
        t = Table([[Paragraph(runs_markup(b.runs), st)]], colWidths=[CONTENT])
        t.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (-1, -1), bg),
            ('BOX', (0, 0), (-1, -1), 0.75, bd),
            ('LEFTPADDING', (0, 0), (-1, -1), 10),
            ('RIGHTPADDING', (0, 0), (-1, -1), 10),
            ('TOPPADDING', (0, 0), (-1, -1), 7),
            ('BOTTOMPADDING', (0, 0), (-1, -1), 7),
        ]))
        return [KeepTogether([t, Spacer(1, 8)])]

    def table_flow(b):
        rows = b.rows
        ncol = max(len(r) for r in rows)
        data = []
        style_cmds = [
            ('GRID', (0, 0), (-1, -1), 0.5, LINE),
            ('VALIGN', (0, 0), (-1, -1), 'TOP'),
            ('LEFTPADDING', (0, 0), (-1, -1), 6),
            ('RIGHTPADDING', (0, 0), (-1, -1), 6),
            ('TOPPADDING', (0, 0), (-1, -1), 4),
            ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ]
        for ri, row in enumerate(rows):
            line = []
            for ci in range(ncol):
                tag, runs = row[ci] if ci < len(row) else ('td', [])
                st = S['cellh'] if tag == 'th' else S['cell']
                line.append(Paragraph(runs_markup(runs), st))
                if tag == 'th':
                    style_cmds.append(('BACKGROUND', (ci, ri), (ci, ri), SOFT2))
                elif ri % 2 == 0:
                    style_cmds.append(('BACKGROUND', (ci, ri), (ci, ri), SOFT))
            data.append(line)
        t = Table(data, colWidths=[CONTENT / ncol] * ncol, repeatRows=1)
        t.setStyle(TableStyle(style_cmds))
        return [KeepTogether([t]), Spacer(1, 6)]

    # cover page drawn on canvas
    def draw_cover(cnv, doc):
        cnv.saveState()
        cnv.setFillColor(BRAND)
        cnv.rect(0, 0, PAGE_W, PAGE_H, stroke=0, fill=1)
        cnv.setFillColor(BRAND2)
        cnv.rect(0, 0, PAGE_W, PAGE_H, stroke=0, fill=0)
        # gradient approximation: overlay lighter band
        cnv.setFillColor(colors.HexColor('#2E5AAC'))
        cnv.setFillAlpha(0.35)
        cnv.rect(PAGE_W * 0.25, 0, PAGE_W * 0.75, PAGE_H, stroke=0, fill=1)
        cnv.setFillAlpha(1)
        cover = next((b for b in blocks if b.kind == 'cover'), None)
        x = M; y = PAGE_H * 0.62
        if cover:
            cnv.setFillColor(colors.HexColor('#C9D6EA'))
            cnv.setFont('YaHei', 9)
            cnv.drawString(x, y, runs_text(cover.badge))
            y -= 44
            cnv.setFillColor(colors.white)
            cnv.setFont('YaHeiB', 40)
            cnv.drawString(x, y, runs_text(cover.title))
            y -= 30
            cnv.setFillColor(colors.HexColor('#DDE6F2'))
            cnv.setFont('YaHei', 17)
            cnv.drawString(x, y, runs_text(cover.sub))
            y -= 34
            cnv.setFont('YaHei', 9.5)
            cnv.setFillColor(colors.HexColor('#C9D6EA'))
            line = []
            meta_lines = []
            for r in cover.meta:
                if r == ('br',):
                    meta_lines.append(line); line = []
                else:
                    line.append(r[0])
            if line: meta_lines.append(line)
            for ln in meta_lines:
                txt = ''.join(t for t in ln if t.strip())
                if not txt: continue
                cnv.drawString(x, y, txt); y -= 16
            y -= 8
            cnv.setFillColor(colors.HexColor('#DDE6F2'))
            # intro as wrapped text
            txt = runs_text(cover.intro)
            from reportlab.lib.utils import simpleSplit
            for ln in simpleSplit(txt, 'YaHei', 9.5, CONTENT * 0.72):
                cnv.drawString(x, y, ln); y -= 15
        cnv.restoreState()

    frame_cover = Frame(0, 0, PAGE_W, PAGE_H, id='cover',
                        leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
    frame_main = Frame(M, M, CONTENT, PAGE_H - 2 * M, id='main')
    doc = BaseDocTemplate(out_path, pagesize=A4,
                          leftMargin=M, rightMargin=M, topMargin=M, bottomMargin=M)
    doc.addPageTemplates([
        PageTemplate(id='Cover', frames=[frame_cover], onPage=draw_cover),
        PageTemplate(id='Main', frames=[frame_main]),
    ])

    story = [NextPageTemplate('Main')]
    # cover placeholder flowable (page drawn by onPage)
    story.append(Spacer(1, 1))  # occupy cover page; onPage callback draws the cover art
    for b in blocks:
        k = b.kind
        if k == 'cover':
            continue
        if k == 'toc':
            story.append(PageBreak())
            story.append(Paragraph('<b>目录</b>',
                ParagraphStyle('toct', fontName='YaHei', fontSize=16, leading=22,
                               textColor=BRAND, spaceAfter=10, wordWrap='CJK')))
            for depth, runs in b.entries:
                st = ParagraphStyle(f'toc{depth}', parent=S['body'],
                                    fontSize=10.5 if depth <= 1 else 10,
                                    leftIndent=0 if depth <= 1 else 20,
                                    spaceAfter=3,
                                    textColor=BRAND if depth <= 1 else colors.HexColor('#33475F'))
                story.append(Paragraph(runs_markup(runs), st))
            continue
        if k == 'h1':
            story.append(PageBreak())
            story.append(Paragraph(runs_markup(b.runs), S['h1']))
            continue
        if k == 'h2':
            story.append(Paragraph(runs_markup(b.runs), S['h2'])); continue
        if k == 'h3':
            story.append(Paragraph(runs_markup(b.runs), S['h3'])); continue
        if k == 'p':
            story.append(Paragraph(runs_markup(b.runs), S['body'])); continue
        if k == 'foot':
            story.append(Paragraph(runs_markup(b.runs), S['foot'])); continue
        if k == 'list':
            n = 0
            for item in b.items:
                if b.lkind == "ol":
                    n += 1
                    story.append(Paragraph(f'<b>{n}.</b> ' + runs_markup(item), S['li']))
                else:
                    story.append(Paragraph('• ' + runs_markup(item), S['li']))
            story.append(Spacer(1, 3))
            continue
        if k == 'table':
            story.extend(table_flow(b)); continue
        if k == 'fig':
            story.extend(fig_flow(b)); continue
        if k == 'callout':
            story.extend(callout_flow(b)); continue

    # coalesce consecutive PageBreaks
    cleaned = []
    for f in story:
        if isinstance(f, PageBreak) and cleaned and isinstance(cleaned[-1], PageBreak):
            continue
        cleaned.append(f)
    doc.build(cleaned)
    return out_path

# ================= main =================
def main():
    blocks = parse_manual()
    print(f"parsed {len(blocks)} blocks:")
    from collections import Counter
    print(" ", Counter(b.kind for b in blocks))
    docx_path = os.path.join(BASE, "SketchCAD用户手册.docx")
    pdf_path = os.path.join(BASE, "SketchCAD用户手册.pdf")
    emit_docx(blocks, docx_path)
    print("DOCX written:", docx_path, os.path.getsize(docx_path), "bytes")
    emit_pdf(blocks, pdf_path)
    print("PDF written:", pdf_path, os.path.getsize(pdf_path), "bytes")

if __name__ == '__main__':
    main()
