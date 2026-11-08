# Minimal JSON reader/writer for the Infinit sidecar.
#
# The sidecar deliberately depends only on the Julia standard library.  The
# protocol is newline-delimited JSON, so a small, predictable parser is enough
# and avoids adding a package that would have to be installed on the user's
# machine.
module MiniJSON

export json_parse, json_write

mutable struct Parser
    s::String
    i::Int
end

function skip_ws(p::Parser)
    while p.i <= lastindex(p.s) && isspace(p.s[p.i])
        p.i = nextind(p.s, p.i)
    end
end

function parse_string(p::Parser)
    if p.s[p.i] != '"'
        error("expected string")
    end
    p.i = nextind(p.s, p.i)
    out = IOBuffer()
    while p.i <= lastindex(p.s)
        c = p.s[p.i]
        p.i = nextind(p.s, p.i)
        if c == '"'
            return String(take!(out))
        elseif c == '\\'
            e = p.s[p.i]
            p.i = nextind(p.s, p.i)
            if e == '"' || e == '\\' || e == '/'
                print(out, e)
            elseif e == 'b'
                print(out, '\b')
            elseif e == 'f'
                print(out, '\f')
            elseif e == 'n'
                print(out, '\n')
            elseif e == 'r'
                print(out, '\r')
            elseif e == 't'
                print(out, '\t')
            elseif e == 'u'
                code = 0
                for _ in 1:4
                    h = p.s[p.i]
                    p.i = nextind(p.s, p.i)
                    code = code * 16 + parse(Int, h, base=16)
                end
                print(out, Char(code))
            else
                print(out, e)
            end
        else
            print(out, c)
        end
    end
    error("unterminated string")
end

function parse_value(p::Parser)
    skip_ws(p)
    if p.i > lastindex(p.s)
        error("unexpected end of JSON")
    end
    c = p.s[p.i]
    if c == '{'
        p.i = nextind(p.s, p.i)
        out = Dict{String,Any}()
        skip_ws(p)
        if p.s[p.i] == '}'
            p.i = nextind(p.s, p.i)
            return out
        end
        while true
            skip_ws(p)
            key = parse_string(p)
            skip_ws(p)
            if p.s[p.i] != ':'
                error("expected ':'")
            end
            p.i = nextind(p.s, p.i)
            out[key] = parse_value(p)
            skip_ws(p)
            if p.s[p.i] == '}'
                p.i = nextind(p.s, p.i)
                return out
            end
            if p.s[p.i] != ','
                error("expected ','")
            end
            p.i = nextind(p.s, p.i)
        end
    elseif c == '['
        p.i = nextind(p.s, p.i)
        out = Any[]
        skip_ws(p)
        if p.s[p.i] == ']'
            p.i = nextind(p.s, p.i)
            return out
        end
        while true
            push!(out, parse_value(p))
            skip_ws(p)
            if p.s[p.i] == ']'
                p.i = nextind(p.s, p.i)
                return out
            end
            if p.s[p.i] != ','
                error("expected ','")
            end
            p.i = nextind(p.s, p.i)
        end
    elseif c == '"'
        return parse_string(p)
    elseif startswith(SubString(p.s, p.i), "true")
        p.i += 4
        return true
    elseif startswith(SubString(p.s, p.i), "false")
        p.i += 5
        return false
    elseif startswith(SubString(p.s, p.i), "null")
        p.i += 4
        return nothing
    else
        start = p.i
        while p.i <= lastindex(p.s) && !(p.s[p.i] in [',', '}', ']', ' ', '\n', '\t', '\r'])
            p.i = nextind(p.s, p.i)
        end
        token = SubString(p.s, start, prevind(p.s, p.i))
        if occursin('.', token) || occursin('e', token) || occursin('E', token)
            return parse(Float64, token)
        end
        return parse(Int, token)
    end
end

json_parse(s::AbstractString) = parse_value(Parser(String(s), 1))

function escape_string(io::IO, s::AbstractString)
    print(io, '"')
    for c in s
        if c == '"'
            print(io, "\\\"")
        elseif c == '\\'
            print(io, "\\\\")
        elseif c == '\n'
            print(io, "\\n")
        elseif c == '\r'
            print(io, "\\r")
        elseif c == '\t'
            print(io, "\\t")
        elseif c < ' '
            print(io, "\\u", string(UInt16(c), base=16, pad=4))
        else
            print(io, c)
        end
    end
    print(io, '"')
end

function write_value(io::IO, x)
    if x === nothing
        print(io, "null")
    elseif x isa Bool
        print(io, x ? "true" : "false")
    elseif x isa Integer
        print(io, x)
    elseif x isa AbstractFloat
        if isfinite(x)
            print(io, repr(x))
        else
            print(io, "null")
        end
    elseif x isa AbstractString
        escape_string(io, x)
    elseif x isa AbstractVector
        print(io, '[')
        for (i, v) in enumerate(x)
            i > 1 && print(io, ',')
            write_value(io, v)
        end
        print(io, ']')
    elseif x isa AbstractDict
        print(io, '{')
        first = true
        for (k, v) in x
            if !first
                print(io, ',')
            end
            first = false
            escape_string(io, string(k))
            print(io, ':')
            write_value(io, v)
        end
        print(io, '}')
    else
        escape_string(io, string(x))
    end
end

function json_write(x)
    io = IOBuffer()
    write_value(io, x)
    return String(take!(io))
end

end # module
