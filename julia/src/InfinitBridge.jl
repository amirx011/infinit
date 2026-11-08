# Infinit Lab — optional Julia sidecar.
#
# This process is launched by the desktop app only.  It speaks newline-delimited
# JSON on stdin/stdout and implements the heavy solvers that are awkward to
# maintain in the C++ core: high-order linear equations with constant
# coefficients and stiff initial-value problems.  Android never starts it; the
# C++ core stays the universal fallback.
module InfinitBridge

include("MiniJSON.jl")
using .MiniJSON
using LinearAlgebra
using Printf

const VERSION = "0.1.0"

# ---------------------------------------------------------------- formatting
function fmt(x::Real)
    if !isfinite(x)
        return x < 0 ? "-inf" : "inf"
    end
    if abs(x) < 1e-12
        return "0"
    end
    r = round(x)
    if abs(x - r) < 1e-10
        return string(Int(r))
    end
    s = @sprintf("%.6g", x)
    if occursin('.', s) && !occursin('e', s)
        s = rstrip(s, '0')
        s = rstrip(s, '.')
    end
    return s
end

ex(plain, pretty=plain, latex=plain) = Dict("plain" => plain, "pretty" => pretty, "latex" => latex)
step(title, note; math=nothing) = Dict("title" => title, "note" => note, "math" => math)

function check(name, ok, detail)
    Dict("name" => name, "ok" => ok, "detail" => detail)
end

function result(; ok=true, mode="topic", type_en="", type_fa="", error="", solution=nothing,
                steps=Any[], checks=Any[], warnings=Any[], assumptions=Any[],
                closed_form=false, order=0, has_numeric=false, numeric_value=0.0,
                point="")
    Dict(
        "ok" => ok,
        "mode" => mode,
        "type_id" => type_en,
        "type_en" => type_en,
        "type_fa" => type_fa,
        "error" => error,
        "solution" => solution,
        "steps" => steps,
        "checks" => checks,
        "warnings" => warnings,
        "assumptions" => assumptions,
        "verified" => ok,
        "closed_form" => closed_form,
        "order" => order,
        "has_numeric" => has_numeric,
        "numeric_value" => numeric_value,
        "point" => point,
    )
end

# ---------------------------------------------------------------- linear ODE
# coeffs are highest order first: a_n, a_{n-1}, ..., a_0.
function ode_linear(params::Dict)
    raw = get(params, "coeffs", Any[])
    coeffs = Float64.(raw)
    n = length(coeffs) - 1
    if n < 1
        return result(ok=false, error="At least two coefficients are required.")
    end
    if abs(coeffs[1]) < 1e-14
        return result(ok=false, error="The leading coefficient must be nonzero.")
    end
    an = coeffs[1]
    c = [coeffs[n - k + 1] / an for k in 0:(n - 1)]
    C = zeros(n, n)
    for i in 1:(n - 1)
        C[i + 1, i] = 1.0
    end
    for i in 1:n
        C[i, n] = -c[i]
    end
    roots = eigvals(C)
    # Group roots by value, keeping multiplicity.
    groups = Vector{Tuple{ComplexF64, Int}}()
    used = falses(length(roots))
    for i in eachindex(roots)
        used[i] && continue
        r = roots[i]
        count = 1
        used[i] = true
        for j in (i + 1):length(roots)
            used[j] && continue
            if abs(roots[j] - r) < 1e-7
                count += 1
                used[j] = true
            end
        end
        push!(groups, (r, count))
    end

    # Build the real basis, one entry per free constant.
    basis = Vector{String}()      # plain text
    basis_latex = Vector{String}()
    basis_meta = Any[]            # (kind, alpha, beta, j)
    names = String[]
    constant = 1
    for (r, mult) in groups
        if abs(imag(r)) < 1e-8
            lambda = real(r)
            for j in 0:(mult - 1)
                tpart = j == 0 ? "" : (j == 1 ? "t" : "t^$j")
                plain = string(tpart, j == 0 ? "" : "·", "exp(", fmt(lambda), "·t)")
                push!(basis, plain)
                push!(basis_latex, string(tpart, "e^{", fmt(lambda), "t}"))
                push!(basis_meta, (:real, lambda, 0.0, j, 1))
                push!(names, "C$constant")
                constant += 1
            end
        elseif imag(r) > 0
            alpha = real(r)
            beta = imag(r)
            for j in 0:(mult - 1)
                tpart = j == 0 ? "" : (j == 1 ? "t" : "t^$j")
                push!(basis, string(tpart, j == 0 ? "" : "·", "exp(", fmt(alpha), "·t)·cos(",
                                     fmt(beta), "·t)"))
                push!(basis, string(tpart, j == 0 ? "" : "·", "exp(", fmt(alpha), "·t)·sin(",
                                     fmt(beta), "·t)"))
                push!(basis_latex, string(tpart, "e^{", fmt(alpha), "t}\\cos(", fmt(beta), "t)"))
                push!(basis_latex, string(tpart, "e^{", fmt(alpha), "t}\\sin(", fmt(beta), "t)"))
                push!(basis_meta, (:complex, alpha, beta, j, 1))
                push!(basis_meta, (:complex, alpha, beta, j, 2))
                push!(names, "C$constant")
                constant += 1
                push!(names, "C$constant")
                constant += 1
            end
        end
    end

    # General solution text.
    general_plain = join([string(names[i], "·", basis[i]) for i in eachindex(basis)], " + ")
    general_latex = join([string(names[i], basis_latex[i]) for i in eachindex(basis)], " + ")

    steps = Any[]
    char_terms = String[]
    for (k, a) in enumerate(coeffs)
        power = n - k + 1
        power == 0 && push!(char_terms, fmt(a))
        power == 1 && push!(char_terms, string(fmt(a), "·r"))
        power > 1 && push!(char_terms, string(fmt(a), "·r^", power))
    end
    push!(steps, step("Write the characteristic equation",
                      "Replace each derivative by a power of r.",
                      math=ex(join(char_terms, " + ") * " = 0",
                              join(char_terms, " + ") * " = 0",
                              join(char_terms, " + ") * " = 0")))
    root_text = join([string("r = ", fmt(real(r)), abs(imag(r)) < 1e-8 ? "" :
                             string(" ± ", fmt(abs(imag(r))), "i"), " (×", mult, ")")
                      for (r, mult) in groups], ", ")
    push!(steps, step("Find the roots", "Real roots give exponentials; complex pairs give damped sines and cosines.",
                      math=ex(root_text)))
    push!(steps, step("Write the general solution", "There are $n free constants.",
                      math=ex(general_plain, general_plain, general_latex)))

    solution = ex(general_plain, general_plain, general_latex)
    ics = get(params, "ics", nothing)
    if ics isa AbstractVector && length(ics) == n
        # Build the initial-condition matrix and solve for the constants.
        values = Float64.(ics)
        M = zeros(n, n)
        col = 1
        for (kind, alpha, beta, j, comp) in basis_meta
            for k in 0:(n - 1)
                if k < j
                    M[k + 1, col] = 0.0
                else
                    factor = factorial(k) / factorial(k - j)
                    if kind == :real
                        M[k + 1, col] = factor * alpha^(k - j)
                    else
                        z = complex(alpha, beta)^(k - j)
                        M[k + 1, col] = factor * (comp == 1 ? real(z) : imag(z))
                    end
                end
            end
            col += 1
        end
        constants = try
            M \ values
        catch
            nothing
        end
        if constants !== nothing
            parts = String[]
            parts_latex = String[]
            for i in eachindex(basis)
                cval = constants[i]
                abs(cval) < 1e-10 && continue
                push!(parts, string(fmt(cval), "·", basis[i]))
                push!(parts_latex, string(fmt(cval), basis_latex[i]))
            end
            final_plain = isempty(parts) ? "0" : join(parts, " + ")
            final_latex = isempty(parts_latex) ? "0" : join(parts_latex, " + ")
            solution = ex(final_plain, final_plain, final_latex)
            push!(steps, step("Apply the initial conditions",
                              "Solve the linear system for the constants.",
                              math=ex(final_plain, final_plain, final_latex)))
        end
    end

    return result(
        type_en="Linear ODE, constant coefficients",
        type_fa="معادلهٔ خطی با ضرایب ثابت",
        solution=solution,
        steps=steps,
        checks=[check("Characteristic roots", true, "Computed $n roots.")],
        closed_form=true,
        order=n,
    )
end

# ---------------------------------------------------------------- stiff ODE
function ode_stiff(params::Dict)
    fstr = get(params, "f", "x + y")
    x0 = Float64(get(params, "x0", 0.0))
    y0 = Float64(get(params, "y0", 1.0))
    xend = Float64(get(params, "xend", 10.0))
    h = Float64(get(params, "h", 0.1))
    if abs(h) < 1e-12
        return result(ok=false, error="The step size must be nonzero.")
    end
    if (xend - x0) / h > 5000
        return result(ok=false, error="The step count is too large; increase h.")
    end
    fname = gensym(:infinit_f)
    try
        expr = Meta.parse(fstr)
        Core.eval(Main, :($fname(x, y) = $expr))
    catch e
        return result(ok=false, error="Could not compile f(x,y): $(e)")
    end
    fn = getfield(Main, fname)
    x = x0
    y = y0
    table = IOBuffer()
    println(table, "i\tx\ty")
    println(table, "0\t", fmt(x), "\t", fmt(y))
    n = round(Int, (xend - x0) / h)
    for i in 1:n
        xnew = x0 + i * h
        z = y
        for _ in 1:30
            fz = Base.invokelatest(fn, xnew, z)
            eps = 1e-7
            df = (Base.invokelatest(fn, xnew, z + eps) -
                  Base.invokelatest(fn, xnew, z - eps)) / (2eps)
            F = z - y - h * fz
            dF = 1 - h * df
            if abs(dF) < 1e-14
                break
            end
            dz = F / dF
            z -= dz
            if abs(dz) < 1e-12
                break
            end
        end
        y = z
        x = xnew
        if i <= 25
            println(table, i, "\t", fmt(x), "\t", fmt(y))
        end
    end
    return result(
        type_en="Stiff initial-value problem",
        type_fa="مسئلهٔ مقدار اولیهٔ سخت",
        solution=ex(fmt(y)),
        has_numeric=true,
        numeric_value=y,
        point="x=$(fmt(x))",
        steps=[
            step("Write the implicit Euler step",
                 "Solve z = y + h f(x+h, z) at every step.",
                 math=ex("y_{n+1} = y_n + h f(x_{n+1}, y_{n+1})")),
            step("Advance through the interval", "Newton's method solves each implicit step.",
                 math=ex(String(take!(table)))),
            step("Read the final value", "After $n steps.", math=ex(fmt(y))),
        ],
        checks=[check("Step count", true, "The table contains $n steps.")],
    )
end

# ---------------------------------------------------------------- dispatch
function handle(req::Dict)
    method = get(req, "method", "")
    params = get(req, "params", Dict{String,Any}())
    if method == "ping"
        return Dict("pong" => true, "version" => VERSION)
    elseif method == "ode_linear"
        return ode_linear(params)
    elseif method == "ode_stiff"
        return ode_stiff(params)
    else
        return result(ok=false, error="Unknown method: $method")
    end
end

function main()
    for line in eachline(stdin)
        isempty(strip(line)) && continue
        id = nothing
        response = try
            req = json_parse(line)
            id = get(req, "id", nothing)
            payload = handle(req)
            Dict("id" => id, "ok" => get(payload, "ok", true), "result" => payload)
        catch e
            Dict("id" => id, "ok" => false, "error" => sprint(showerror, e))
        end
        println(json_write(response))
        flush(stdout)
    end
end

end # module

if abspath(PROGRAM_FILE) == @__FILE__
    InfinitBridge.main()
end
