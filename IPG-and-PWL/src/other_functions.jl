function parametrized_expressions(alphas, NL_term = "inverse_square_root")
    # return an expression with alpha as parameter by writing it in a file and reading
    # because it raises an error to write simply expr = :(alpha*(1/sqrt(1-x))-1)
    file = open("expressions.jl", "w")
    println(file, "expr_h = []")
    for i in 1:length(alphas)
        alpha = alphas[i]
        if NL_term == "inverse_square_root"
            println(file, "push!(expr_h, :($(alpha)*(1/sqrt(1-x)-1)))")
        elseif NL_term == "inverse_cubic_root"
            println(file, "push!(expr_h, :($(alpha)*(1/(1-x)^(1/3)-1)))")
        elseif NL_term == "log"
            println(file, "push!(expr_h, :($(alpha)*(-log(1-x))))")
        elseif NL_term == "cube+inverse_square_root"
            println(file, "push!(expr_h, :($(alpha)*(1/sqrt(1-x)-1+3/2*x^3)))")
        elseif NL_term == "S+inverse_square_root"
            println(file, "push!(expr_h, :($(alpha)*(1/sqrt(1-x)-2+2/(1+exp(-20*x)))))")
        end
    end
    close(file)
    return 0
end

function parametrized_quadratic_expression(coef)
    # return an expression with alpha as parameter by writing it in a file and reading
    # because it raises an error to write simply expr = :($coef*x^2)
    file = open("quadratic_expression.jl", "w")
    println(file, "expr_quad = :($coef*x^2)")
    close(file)
    return 0
end

function add_string_error(name,err)
    # add to the string name an information on err: "Abs" or "Rel" followed by "{integer_part}-{floating_part}"
    # add type of error
    if typeof(err) == Absolute
        name = string(name, "Abs")
        val = err.delta
    elseif typeof(err) == Relative
        name = string(name, "Rel")
        val = err.percent
    else
        error("unknown type of error: $(typeof(err))")
    end
    # remove a null decimal part for unicity reasons
    if val == floor(val)
        val = Int(floor(val))
    end
    # add value of error
    name = string(name,"$(replace(string(val), "."=>"-"))")
    return string(name, "_")
end

function compute_filename(filename_instance, err1, err2, err3, fixed_cost)
    # return the name of the instance according to the parameters
    name = ""
    name = string(name, filename_instance[1:end-4], "_") # removing .txt
    name = add_string_error(name,err1)
    name = add_string_error(name,err2)
    name = add_string_error(name,err3)
    name = string(name, "fixedcost$fixed_cost/model.txt") # do not change "model" again, or change everywhere it will change something
    return name
end
