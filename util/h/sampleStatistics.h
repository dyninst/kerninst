// sampleStatistics.h
// Assumes that each sample is an integer value.

#ifndef _SAMPLE_STATISTICS_H_
#define _SAMPLE_STATISTICS_H_

#include <inttypes.h>
#include <math.h>
#include "common/h/Vector.h"

class sampleStatistics {
 private:
   unsigned n; // number of samples
   uint64_t sum;
   uint64_t sumOfSquares;
   uint32_t max;

   double getConfidenceIntervalPlusMinusFromTheMean(double Z) const;

 public:
   sampleStatistics() {
      n = 0;
      sum = 0;
      sumOfSquares = 0;
      max = 0;
   }

   void operator+=(const sampleStatistics &other);

   void addSample(uint32_t sampleValue);

   uint32_t getMax() const {
      return max;
   }
   unsigned getNumSamples() const {
      return n;
   }
   uint64_t getSum() const {
      return sum;
   }
   
   double getMean() const {
      return (double)sum / (double)n;
   }

   double getSampleVariance() const;
   double getSampleStdDeviation() const;

   double getVarianceOfTheMean() const; // sample variance divided by n
   double getStdDeviationOfTheMean() const; // sample std deviation divided by sqrt(n)
   
   double getConfidenceInterval95PlusMinusFromTheMean() const {
      return getConfidenceIntervalPlusMinusFromTheMean(1.96);
   }
};

#endif
