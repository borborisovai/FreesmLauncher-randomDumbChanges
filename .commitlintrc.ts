// NOTE: dependency-free config so it loads with any commitlint version
// (kept in sync with the previous @commitlint/types-based version)
enum Severity {
	Warning = 1,
	Error = 2,
}
type Condition = 'always' | 'never';
type Case =
	| 'lower-case'
	| 'sentence-case'
	| 'start-case'
	| 'pascal-case'
	| 'upper-case';

export default {
	parserPreset: 'conventional-changelog-conventionalcommits',
	rules: {
		'body-leading-blank': [Severity.Warning, 'always'] as const,
		'body-max-line-length': [Severity.Error, 'always', 70] as const,
		'footer-leading-blank': [Severity.Warning, 'always'] as const,
		'footer-max-line-length': [
			Severity.Error,
			'always',
			70,
		] as const,
		'header-max-length': [Severity.Error, 'always', 70] as const,
		'header-trim': [Severity.Error, 'always'] as const,
		'subject-case': [
			Severity.Error,
			'never',
			['sentence-case', 'start-case', 'pascal-case', 'upper-case'],
		] as [Severity, Condition, Case[]],
		'subject-empty': [Severity.Error, 'never'] as const,
		'subject-full-stop': [Severity.Error, 'never', '.'] as const,
		'type-case': [Severity.Error, 'always', 'lower-case'] as const,
		'type-empty': [Severity.Error, 'never'] as const,
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
		] as [Severity, Condition, string[]],
	},
};
