#! /usr/bin/env python

import os
import re
import sys
import shutil
import argparse
from collections import defaultdict

INDEX_HTML = """
<html>
<head>
<title>build - scan-build results</title>
<link type="text/css" rel="stylesheet" href="scanview.css"/>
<script src="sorttable.js"></script>
<script language='javascript' type="text/javascript">
function SetDisplay(RowClass, DisplayVal)
{
  var Rows = document.getElementsByTagName("tr");
  for ( var i = 0 ; i < Rows.length; ++i ) {
    if (Rows[i].className == RowClass) {
      Rows[i].style.display = DisplayVal;
    }
  }
}

function CopyCheckedStateToCheckButtons(SummaryCheckButton) {
  var Inputs = document.getElementsByTagName("input");
  for ( var i = 0 ; i < Inputs.length; ++i ) {
    if (Inputs[i].type == "checkbox") {
      if(Inputs[i] != SummaryCheckButton) {
        Inputs[i].checked = SummaryCheckButton.checked;
        Inputs[i].onclick();
      }
    }
  }
}

function returnObjById( id ) {
    if (document.getElementById)
        var returnVar = document.getElementById(id);
    else if (document.all)
        var returnVar = document.all[id];
    else if (document.layers)
        var returnVar = document.layers[id];
    return returnVar;
}

var NumUnchecked = 0;

function ToggleDisplay(CheckButton, ClassName) {
  if (CheckButton.checked) {
    SetDisplay(ClassName, "");
    if (--NumUnchecked == 0) {
      returnObjById("AllBugsCheck").checked = true;
    }
  }
  else {
    SetDisplay(ClassName, "none");
    NumUnchecked++;
    returnObjById("AllBugsCheck").checked = false;
  }
}
</script>
<!-- SUMMARYENDHEAD -->
</head>
<body>
<h1>build - scan-build results</h1>

<table>
<tr><th>User:</th><td></td></tr>
<tr><th>Working Directory:</th><td></td></tr>
<tr><th>Command Line:</th><td></td></tr>
<tr><th>Clang Version:</th><td></td></tr>
<tr><th>Date:</th><td></td></tr>
</table>
<h2>Bug Summary</h2><table>
<thead><tr><td>Bug Type</td><td>Quantity</td><td class="sorttable_nosort">Display?</td></tr></thead>
<tr style="font-weight:bold"><td class="SUMM_DESC">All Bugs</td><td class="Q">%(BUGS_TOTAL)s</td><td><center><input type="checkbox" id="AllBugsCheck" onClick="CopyCheckedStateToCheckButtons(this);" checked/></center></td></tr>
<tr><th>Logic error</th><th colspan=2></th></tr>
%(BUG_SUMMARIES)s
</table>
<h2>Reports</h2>

<table class="sortable" style="table-layout:automatic">
<thead><tr>
  <td>Bug Group</td>
  <td class="sorttable_sorted">Bug Type<span id="sorttable_sortfwdind">&nbsp;&#x25BE;</span></td>
  <td>File</td>
  <td>Function/Method</td>
  <td class="Q">Line</td>
  <td class="Q">Path Length</td>
  <td class="sorttable_nosort"></td>
  <!-- REPORTBUGCOL -->
</tr></thead>
<tbody>
%(BUG_REPORTS)s
<!-- REPORTBUG id="report-cb9dae.html" -->
</tr>
</tbody>
</table>

</body></html>
"""

# BUG_SUMMARIES is a list of these:
# <tr><td class="SUMM_DESC">Dereference of null pointer</td><td class="Q">1</td><td><center><input type="checkbox" onClick="ToggleDisplay(this,'bt_logic_error_dereference_of_null_pointer');" checked/></center></td></tr>
BUG_SUMMARY_FMT="""<tr><td class="SUMM_DESC">%(BUG_TYPE)s</td><td class="Q">%(BUG_TYPE_COUNT)s</td><td><center><input type="checkbox" onClick="ToggleDisplay(this,'%(BUG_CLASS)s');" checked/></center></td></tr>"""

# BUG_REPORTS is a list of these:
# <tr class="bt_logic_error_dereference_of_null_pointer"><td class="DESC">Logic error</td><td class="DESC">Dereference of null pointer</td><td>easing.cpp</td><td class="DESC">GetValue</td><td class="Q">25</td><td class="Q">5</td><td><a href="report-cb9dae.html#EndPath">View Report</a></td>
BUG_REPORT_FMT="""<tr class="%(BUG_CLASS)s"><td class="DESC">%(BUG_GROUP)s</td><td class="DESC">%(BUG_TYPE)s</td><td>%(FILE_NAME)s</td><td class="DESC">%(FUNCTION_NAME)s</td><td class="Q">%(LINE_NUMBER)s</td><td class="Q">%(LENGTH)s</td><td><a href="%(REPORT_FILE)s#EndPath">View Report</a></td><!-- REPORTBUG id="%(REPORT_FILE)s" --></tr>"""
BUG_REPORT_RE="<tr class=\"(.*?)\"><td class=\"DESC\">(.*?)</td><td class=\"DESC\">(.*?)</td><td>(.*?)</td><td class=\"DESC\">(.*?)</td><td class=\"Q\">([0-9]+?)</td><td class=\"Q\">([0-9]+?)</td><td><a href=\"(.*?)#EndPath\">View Report</a></td>"

def get_report_info(report):
    filename_re = re.compile(r"<title>(.*)</title>")
    with open(report, 'rb') as f:
        text = f.read()

    filepath = filename_re.findall(text)
    if not filepath:
        return '<unknown>'
    return filepath[0]


def get_index_info(index):
    with open(index, 'rb') as f:
        text = f.read()

    buginfo_re = re.compile(BUG_REPORT_RE)
    bugs = buginfo_re.findall(text)
    out = []
    for bug in bugs:
        d = dict()
        d['BUG_CLASS'] = bug[0]
        d['BUG_GROUP'] = bug[1]
        d['BUG_TYPE'] = bug[2]
        d['FILE_NAME'] = bug[3]
        d['FUNCTION_NAME'] = bug[4]
        d['LINE_NUMBER'] = bug[5]
        d['LENGTH'] = bug[6]
        d['REPORT_FILE'] = bug[7]
        out.append(d)
    return out

def copy_file(srcdir, dstdir, name):
    dst = os.path.join(dstdir, name)
    src = os.path.join(srcdir, name)
    shutil.copy2(src, dst)

def handle_report(context, targetdir, report, index):
    srcdir = os.path.dirname(report)
    copy_file(srcdir, targetdir, os.path.basename(report))

    # get the filename from the report
    filepath = get_report_info(report)
    bugs = get_index_info(index)

    for bug in bugs:
        bug['FILE_NAME'] = filepath
        context.append( (filepath, bug['LINE_NUMBER'], bug) )


def generate_output(targetdir, bugs):
    # gather the total bug counts
    class_counts = defaultdict(int)
    class_to_type = defaultdict(str)
    for _, _, bug in bugs:
        class_counts[bug['BUG_CLASS']] += 1
        class_to_type[bug['BUG_CLASS']] = bug['BUG_TYPE']

    # store the bug count with 
    BUG_SUMMARIES = ""
    for cls, count in class_counts.iteritems():
        BUG_SUMMARIES += BUG_SUMMARY_FMT % {'BUG_CLASS':cls, 'BUG_TYPE_COUNT':'%d'%count, 'BUG_TYPE':class_to_type[cls]} + '\n'
    
    BUG_REPORTS = ""
    for _, _, bug in bugs:
        BUG_REPORTS += BUG_REPORT_FMT % bug + '\n'
    text = INDEX_HTML % {'BUGS_TOTAL': '%d' % len(bugs), 'BUG_SUMMARIES': BUG_SUMMARIES, 'BUG_REPORTS' : BUG_REPORTS }
    with open(os.path.join(targetdir, 'index.html'), 'wb') as f:
        f.write(text)
        f.write('\n')


def gather_reports(targetdir, searchdir):
    bugs = []
    for root,dirs,files in os.walk(searchdir):
        for f in files:
            if f.startswith('report-') and f.endswith('.html'):
                report = os.path.join(root,f)
                index = os.path.join(root,'index.html')
                if os.path.exists(index):
                    handle_report(bugs, targetdir, report, index)
                    copy_file(root, targetdir, 'scanview.css')
                    copy_file(root, targetdir, 'sorttable.js')
    return bugs


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Gathers scan-build reports and merges them into one')
    parser.add_argument('-i', '--input', help='The input file/directory with folders containing reports')
    parser.add_argument('-o', '--output', default='.', help='The output directory')
    args = parser.parse_args()

    output_dir = args.output
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    bugs = gather_reports(output_dir, args.input)
    generate_output(output_dir, bugs)
