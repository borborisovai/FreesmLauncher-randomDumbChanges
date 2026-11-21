// NOTE: plain CommonJS so it loads with any commitlint version, no TS syntax
const Severity = { Warning: 1, Error: 2 };

module.exports = {
	parserPreset: 'conventional-changelog-conventionalcommits',
	rules: {
		'body-leading-blank': [Severity.Warning, 'always'],
		'body-max-line-length': [Severity.Error, 'always', 70],
		'footer-leading-blank': [Severity.Warning, 'always'],
		'footer-max-line-length': [
			Severity.Error,
			'always',
			100,
		],
		'header-max-length': [Severity.Error, 'always', 70],
		'header-trim': [Severity.Error, 'always'],
		'subject-case': [
			Severity.Error,
			'never',
			['sentence-case', 'start-case', 'pascal-case', 'upper-case'],
		],
		'subject-empty': [Severity.Error, 'never'],
		'subject-full-stop': [Severity.Error, 'never', '.'],
		'type-case': [Severity.Error, 'always', 'lower-case'],
		'type-empty': [Severity.Error, 'never'],
		'type-enum': [
			Severity.Error,
			'always',
			[
				'build',
				'chore',
				'ci',
				'docs',
				'feat',
				'fix',
				'perf',
				'refactor',
				'revert',
				'style',
				'test',
			],
		],
	},
};
