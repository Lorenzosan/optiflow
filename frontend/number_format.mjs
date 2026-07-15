export function formatNumber(value, maximumFractionDigits = 2, locale = undefined) {
  const numeric = Number(value);
  if (value === null || value === undefined || !Number.isFinite(numeric)) {
    return "—";
  }
  if (!Number.isInteger(maximumFractionDigits) || maximumFractionDigits < 0) {
    throw new Error("maximumFractionDigits must be a non-negative integer");
  }

  const zeroThreshold = 0.5 * (10 ** -maximumFractionDigits);
  const normalized = Math.abs(numeric) < zeroThreshold ? 0 : numeric;
  return new Intl.NumberFormat(locale, {
    maximumFractionDigits,
  }).format(normalized);
}
