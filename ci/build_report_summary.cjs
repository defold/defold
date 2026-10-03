// Build one run summary from artifact metadata without downloading build outputs.
module.exports = function buildReportSummary(artifacts, {serverUrl, owner, repo, runId, attempt}) {
    const suffix = `-${attempt}`;
    const reports = artifacts.filter(a => !a.expired &&
        /^build-reports-[a-z0-9_-]+-\d+$/.test(a.name) && a.name.endsWith(suffix))
        .sort((a, b) => a.name.localeCompare(b.name));
    const lines = ['## Build Reports', ''];
    if (!reports.length) {
        return lines.concat('No build reports were uploaded for this attempt. See the engine job logs.', '').join('\n');
    }
    for (const report of reports) {
        lines.push(`- ${report.name} — [Download](${serverUrl}/${owner}/${repo}/actions/runs/${runId}/artifacts/${report.id})`);
    }
    lines.push('', 'Report paths are preserved inside each bundle. Open HTML reports in your browser. Artifacts expire after 14 days.',
        '', '### How to download manually', '', '```sh',
        `gh run download ${runId} --repo ${owner}/${repo} --pattern 'build-reports-*-${attempt}' --dir build-reports`,
        '```', '');
    return lines.join('\n');
};
