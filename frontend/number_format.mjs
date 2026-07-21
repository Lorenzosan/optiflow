/**
 * @file
 * @brief Locale-aware numeric formatting shared by frontend views.
 */

/**
 * @brief Formats a finite numeric value with bounded fractional precision.
 * @param value Value to format.
 * @param maximumFractionDigits Maximum displayed decimal places.
 * @param locale Optional locale override for `Intl.NumberFormat`.
 * @return A localized number or an em dash for non-finite input.
 */
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
