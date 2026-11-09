#!/usr/bin/env python3
"""
Convert lcov summary info or output into a clean GitHub Flavored Markdown report.
Can read directly from tests/build_cov/coverage_filtered.info or stdin.
"""

import sys
import os
import re
from datetime import datetime

def parse_info_file(filepath):
    """
    Parses lcov .info file records:
    SF:<path>
    LF:<lines_found>
    LH:<lines_hit>
    FNF:<funcs_found>
    FNH:<funcs_hit>
    end_of_record
    """
    files = []
    current = {}
    
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = line.strip()
            if line.startswith('SF:'):
                current = {'file': line[3:]}
            elif line.startswith('LF:'):
                current['lines_found'] = int(line[3:])
            elif line.startswith('LH:'):
                current['lines_hit'] = int(line[3:])
            elif line.startswith('FNF:'):
                current['funcs_found'] = int(line[4:])
            elif line.startswith('FNH:'):
                current['funcs_hit'] = int(line[4:])
            elif line == 'end_of_record':
                if 'file' in current:
                    files.append(current)
                current = {}
                
    return files

def generate_markdown(files, output_path=None):
    total_lf = sum(f.get('lines_found', 0) for f in files)
    total_lh = sum(f.get('lines_hit', 0) for f in files)
    total_fnf = sum(f.get('funcs_found', 0) for f in files)
    total_fnh = sum(f.get('funcs_hit', 0) for f in files)
    
    line_cov_pct = (total_lh / total_lf * 100.0) if total_lf > 0 else 0.0
    func_cov_pct = (total_fnh / total_fnf * 100.0) if total_fnf > 0 else 0.0
    
    # Sort files by line coverage ascending to easily spot low coverage areas
    sorted_files = sorted(
        files,
        key=lambda x: (x.get('lines_hit', 0) / x.get('lines_found', 1) if x.get('lines_found', 0) > 0 else 1.0)
    )

    now_str = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    md = []
    md.append("# 📊 LCOV Code Coverage Report")
    md.append("")
    md.append(f"> **Generated at:** `{now_str}`  ")
    md.append(f"> **Target Architecture:** STM32U585VIT6 + ADE9000 (DLMS/COSEM & TrustZone-M)")
    md.append("")
    md.append("## 📌 Executive Summary")
    md.append("")
    md.append("| Metric | Hit / Total | Coverage Percentage | Threshold / Target | Status |")
    md.append("| :--- | :---: | :---: | :---: | :---: |")
    
    line_badge = "🟢 **PASS**" if line_cov_pct >= 75.0 else "🔴 **FAIL**"
    func_badge = "🟢 **PASS**" if func_cov_pct >= 85.0 else "🔴 **FAIL**"
    
    md.append(f"| **Line Coverage** | `{total_lh}` / `{total_lf}` | **{line_cov_pct:.1f}%** | &ge; 75.0% | {line_badge} |")
    md.append(f"| **Function Coverage** | `{total_fnh}` / `{total_fnf}` | **{func_cov_pct:.1f}%** | &ge; 85.0% | {func_badge} |")
    md.append("")
    md.append("---")
    md.append("")
    md.append("## 📁 File Coverage Breakdown")
    md.append("")
    md.append("| Source File | Line Coverage | Lines (Hit/Found) | Function Coverage | Functions (Hit/Found) | Status |")
    md.append("| :--- | :---: | :---: | :---: | :---: | :---: |")
    
    for f in sorted_files:
        lf = f.get('lines_found', 0)
        lh = f.get('lines_hit', 0)
        fnf = f.get('funcs_found', 0)
        fnh = f.get('funcs_hit', 0)
        
        lpct = (lh / lf * 100.0) if lf > 0 else 0.0
        fpct = (fnh / fnf * 100.0) if fnf > 0 else 0.0
        
        # Simplify path
        raw_path = f['file'].replace('\\', '/')
        if '/NonSecure/' in raw_path:
            display_path = 'NonSecure/' + raw_path.split('/NonSecure/')[-1]
        elif '/Secure/' in raw_path:
            display_path = 'Secure/' + raw_path.split('/Secure/')[-1]
        else:
            display_path = os.path.basename(raw_path)
            
        status = "🟢" if lpct >= 80.0 else ("🟡" if lpct >= 60.0 else "🔴")
        md.append(f"| `{display_path}` | **{lpct:.1f}%** | {lh} / {lf} | **{fpct:.1f}%** | {fnh} / {fnf} | {status} |")
        
    md.append("")
    md.append("---")
    md.append("> *Report automatically extracted from LCOV data by `tests/scripts/lcov_to_markdown.py`.*")
    md.append("")
    
    content = "\n".join(md)
    if output_path:
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Coverage Markdown report generated at: {output_path}")
    else:
        print(content)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python3 lcov_to_markdown.py <coverage.info> [output.md]")
        sys.exit(1)
        
    info_file = sys.argv[1]
    out_file = sys.argv[2] if len(sys.argv) > 2 else "COVERAGE.md"
    
    if not os.path.exists(info_file):
        print(f"Error: {info_file} does not exist.")
        sys.exit(1)
        
    parsed = parse_info_file(info_file)
    generate_markdown(parsed, out_file)
