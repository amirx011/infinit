// Infinit Lab — numeric graph sampling, domain/range scans and direction fields.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <map>
#include <string>
#include <vector>

namespace ode {

struct SampleOutput {
    bool ok = false;
    std::string error;
    std::vector<double> xs;
    std::vector<double> ys; // NaN marks an undefined sample
};

// Evaluates `input` on a uniform grid. Constants (for example the C in a
// solution family) are substituted before sampling.
SampleOutput sampleFunction(const std::string& input, const std::string& x, double xmin,
                            double xmax, int count,
                            const std::map<std::string, double>& constants = {});

struct Interval {
    double lo = 0;
    double hi = 0;
    bool loInfinite = false;
    bool hiInfinite = false;
    bool loOpen = true;
    bool hiOpen = true;
};

struct DomainOutput {
    bool ok = false;
    std::string error;
    std::vector<Interval> domain;
    std::vector<Interval> range;
    std::vector<double> holes;
    std::vector<double> asymptotes;
    std::vector<double> rangeHoles;
    std::vector<double> xs;
    std::vector<double> ys;
    bool rangeUnboundedBelow = false;
    bool rangeUnboundedAbove = false;
    bool splitAtZero = false;
    // Short machine-readable classification of the expression, e.g. "rational".
    std::string kind;
};

// A numeric scan of the natural domain and range over [xmin, xmax]. The result
// is honest about being sampled: the client labels it as a numeric estimate.
DomainOutput analyseDomain(const std::string& input, const std::string& x, double xmin,
                           double xmax, int samples);

struct FieldCurve {
    bool ok = false;
    double icX = 0;
    double icY = 0;
    std::vector<double> xs;
    std::vector<double> ys;
};

struct FieldOutput {
    bool ok = false;
    std::string error;
    std::string rhs;
    double xmin = -6;
    double xmax = 6;
    double ymin = -6;
    double ymax = 6;
    int cols = 0;
    int rows = 0;
    std::vector<double> slopes; // row-major; NaN marks a singular point
    std::vector<FieldCurve> curves;
};

// Direction field for a first-order equation y' = f(x, y), together with RK4
// trajectories through the supplied (or automatically chosen) initial points.
FieldOutput analyseField(const std::string& input, const std::string& x, const std::string& y,
                         double xmin, double xmax, double ymin, double ymax, int cols,
                         int rows, int curveCount, const std::vector<std::pair<double, double>>& ics);

struct TrajectorySeed {
    double x0 = 0;
    double y0 = 0;
    double v0 = 0; // y'(x0)
};

struct TrajectoryOutput {
    bool ok = false;
    std::string error;
    std::string rhs; // isolated highest derivative
    int order = 2;
    double xmin = -6;
    double xmax = 6;
    std::vector<FieldCurve> curves;
};

// Numeric trajectories for an equation y^(n) = f(x, y, y', ...). The equation
// is reduced to a first-order system and integrated with RK4 in both directions
// from every seed. This is the honest fallback when no closed form is available,
// and it also draws the solution family for higher-order equations whose exact
// answer is implicit. Orders 2 to 6 are supported.
TrajectoryOutput analyseTrajectory(const std::string& input, const std::string& x,
                                   const std::string& y, double xmin, double xmax,
                                   int curveCount,
                                   const std::vector<TrajectorySeed>& seeds);

struct SurfaceOutput {
    bool ok = false;
    std::string error;
    double xmin = -4;
    double xmax = 4;
    double ymin = -4;
    double ymax = 4;
    int nx = 0;
    int ny = 0;
    std::vector<double> zs; // row-major; NaN marks an undefined sample
};

// Samples z = f(x, y) on a rectangular grid for a contour/heat map.
SurfaceOutput sampleSurface(const std::string& input, const std::string& x,
                            const std::string& y, double xmin, double xmax, double ymin,
                            double ymax, int nx, int ny);

} // namespace ode
