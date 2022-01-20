// sampleStatistics.C

#include "util/h/sampleStatistics.h"

double sampleStatistics::getConfidenceIntervalPlusMinusFromTheMean(double Z) const {
   // plus or minus the following from the mean: (Z * sample_std_dev)/sqrt(n)
   const double std_dev = getSampleStdDeviation();
   const double sqrt_n = sqrt((double)n);

   const double plus_minus = Z * std_dev / sqrt_n;
   return plus_minus;
   // the confidence interval is: [mean-plus_minus, mean+plus_minus]
}

void sampleStatistics::operator+=(const sampleStatistics &other) {
   n += other.n;
   sum += other.sum;
   sumOfSquares += other.sumOfSquares;
   if (other.max > max)
      max = other.max;
}

void sampleStatistics::addSample(uint32_t sampleValue) {
   ++n;
   sum += sampleValue;

   const uint64_t sampleValueAs64 = sampleValue;
   const uint64_t squareOfSampleValue = sampleValueAs64*sampleValueAs64;
   sumOfSquares += squareOfSampleValue;

   if (sampleValue > max)
      max = sampleValue;
}

double sampleStatistics::getSampleVariance() const {
   // sum of squares minus (square of the sum / n), all divided by n-1
   // To avoid fp roundup until the last moment: multiply both the numerator
   // and denominator by n, to get
   // n*sum of squares minus square of the sum, all dividied by n(n-1)

   const uint64_t numerator = n*sumOfSquares - sum*sum;
   const uint64_t denominator = n*(n-1);

   const double numerator_asdouble = numerator;
   const double denominator_asdouble = denominator;
      
   return numerator_asdouble / denominator_asdouble;
}

double sampleStatistics::getSampleStdDeviation() const {
   double result = getSampleVariance();
   return sqrt(result); // slow
}

double sampleStatistics::getVarianceOfTheMean() const {
   return getSampleVariance() / (double)n;
}

double sampleStatistics::getStdDeviationOfTheMean() const {
   return sqrt(getVarianceOfTheMean());
}
