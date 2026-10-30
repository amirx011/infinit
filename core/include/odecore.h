// ODE Lab — C ABI for the exact differential-equation engine.
// SPDX-License-Identifier: MIT
#ifndef ODE_LAB_ODECORE_H
#define ODE_LAB_ODECORE_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define ODE_API __declspec(dllexport)
#else
#define ODE_API __attribute__((visibility("default")))
#endif

/**
 * Solve a request encoded as JSON and return a newly allocated UTF-8 JSON
 * string.  The caller owns the result and must release it with ode_free().
 *
 * Request examples:
 *   {"mode":"ode","input":"y' = y"}
 *   {"mode":"wronskian","functions":["sin(x)","cos(x)"],"x":"x"}
 *   {"mode":"envelope","input":"y = c*x + c^2","c":"c","orthogonal":false}
 */
ODE_API char* ode_solve(const char* request_json);

/** Release a string returned by ode_solve(). */
ODE_API void ode_free(char* text);

/** Engine version, e.g. "0.1.0". */
ODE_API const char* ode_core_version(void);

#ifdef __cplusplus
}
#endif

#endif // ODE_LAB_ODECORE_H
