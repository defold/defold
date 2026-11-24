// Build one run summary from artifact metadata without downloading build outputs.
module.exports = function buildReportSummary(artifacts, {serverUrl, owner, repo, runId}) {
    const latest = new Map();
    for (const artifact of artifacts) {
        const match = /^build-reports-([a-z0-9_-]+?)(?:-(available|partial|unavailable))?-(\d+)$/.exec(artifact.name);
        if (!match) continue;
        const [, target, status, attemptText] = match;
        const attempt = Number(attemptText);
        const previous = latest.get(target);
        if (!previous || attempt > previous.attempt || (attempt === previous.attempt && artifact.id > previous.artifact.id)) {
            latest.set(target, {artifact, target, status, attempt});
        }
    }
    const reports = [...latest.values()].sort((a, b) => a.target.localeCompare(b.target));
    const lines = ['## Build Reports', ''];
    if (!reports.length) {
        return lines.concat('No build reports were uploaded for this run. See the engine job logs.', '').join('\n');
    }
    const descriptions = {
        available: 'Report available',
        partial: 'Some reports unavailable; see the engine job log',
        unavailable: 'Report unavailable; bundle contains diagnostic text only',
    };
    for (const {artifact, status, attempt} of reports) {
        const description = descriptions[status] || 'Report availability not recorded (older upload)';
        const link = artifact.expired ? 'Artifact expired' :
            `[Download](${serverUrl}/${owner}/${repo}/actions/runs/${runId}/artifacts/${artifact.id})`;
        lines.push(`- ${artifact.name} — ${description} · attempt ${attempt} · ${link}`);
    }
    lines.push('', 'Latest upload for each platform/build variant is shown, including jobs retained from earlier attempts.',
        'Report paths are preserved inside each bundle. Open HTML reports in your browser. Artifacts expire after 14 days.');
    const downloadable = reports.filter(report => !report.artifact.expired);
    if (downloadable.length) {
        const names = downloadable.map(({artifact}) => `--name '${artifact.name}'`).join(' ');
        lines.push('', '### How to download manually', '', '```sh',
            `gh run download ${runId} --repo ${owner}/${repo} ${names} --dir build-reports`, '```');
    }
    return lines.concat('').join('\n');
};
