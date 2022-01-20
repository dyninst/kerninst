// binomialDistribStats.h
// provides confidence interval stats for a binomial distribution; i.e. a sample of
// boolean values, rather than a sample of real numbers (compare to sampleStatistics.h)

#ifndef _BINOMIAL_DISTRIB_STATS_H_
#define _BINOMIAL_DISTRIB_STATS_H_

#include <math.h> // sqrt()

class binomialDistribStats {
 private:
   unsigned n; // number of samples reported
   unsigned num_true; // of these, how many were true (rest were false)
   
   double getConfidenceIntervalPlusMinusFromTheSampleProbability(double Z) const {
      // z(alpha/2) * sqrt(pq / n) where q = 1-p
      // Note the "/n", not "*n" that you get with getSampleStdDev().  Hmmm.
      
      const double plus_minus = Z * getEstimateOfStdDevOfEstimator();
      return plus_minus;
   }
   
 public:
   binomialDistribStats() {
      n = 0;
      num_true = 0;
   }

   void operator+=(const binomialDistribStats &other) {
      n += other.n;
      num_true += other.num_true;
   }

   void addSample(bool one_sample) {
      ++n;
      if (one_sample)
         ++num_true;
   }

   double getSampleProbability() const {
      return (double)num_true / (double)n;
   }

   double getSampleVariance() const {
      // np(1-p).
      // See Devore, 3d edition, p110.
      const double p = getSampleProbability();
      const double q = (double)1.0 - p;
      return n*p*q;
   }

   double getSampleStdDev() const {
      return sqrt(getSampleVariance());
   }

   double getVarianceOfEstimator() const {
      // Devore, 3d edition. p 268: pq/n
      const double p = getSampleProbability();
      const double q = (double)1.0 - p;
      return p*q/(double)n;
   }
   double getEstimateOfStdDevOfEstimator() const {
      // std dev of estimator is sqrt(pq/n) where p=(unknown) population proportion.
      // estimator thereof uses x/n for p, above...good enough for me.
      // Used in confidence intervals.
      return sqrt(getVarianceOfEstimator());
   }

   pair<double, bool> getConfidenceInterval95ForAPopulationProportion() const {
      // result.first: the +/- value to apply to getSampleProbability() result.
      // result.second: true iff the value can be trusted; i.e. if the sample size
      // was large enough.
      pair<double, bool> result;

      const double p = getSampleProbability();
      const double q = (double)1.0 - p;
      
      result.first = getConfidenceIntervalPlusMinusFromTheSampleProbability(1.96);
      result.second = ((double)n * p) >= 5 && ((double)n * q) >= 5;

      return result;
   }
   
};

#endif
