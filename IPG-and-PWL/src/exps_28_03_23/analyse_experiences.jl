using Plots
using LaTeXStrings

function find_comparable_experiences(i, experiences, option, option_values)
    # find the set of indices of experiences which have the same setting than experiences[i] except for option which is in option_values
    # those indices corresponds to the order of option_values

    # special case: experiences[i] has an error (ERROR written in outputs.solution or outputs.delta_eq == 1e12)
    if experiences[i].outputs.solution == [[]] || experiences[i].outputs.delta_eq == 1e12
        return []
    end

    list_of_options = fieldnames(option_cs_instance)
    comparable_instances = Any["UNK" for i in 1:length(option_values)]
    comparable_instances[1] = i
    #println("starting find_comparable_experiences with index $i and exp:\n$(experiences[i].options) ")

    for ind in 1:length(experiences)
        exp = experiences[ind]
        #println("currently examining index $ind with options $(exp.options)")
        is_comparable = true
        for field in list_of_options
            if field == option
                if !(getfield(exp.options, field) in option_values) || getfield(exp.options, field) == option_values[1]
                    #println("index $ind is false with field = $field: first test $(!(getfield(exp.options, field) in option_values)), second test $(getfield(exp.options, field) == option_values[1])")
                    is_comparable = false
                    break
                end
            else
                if getfield(exp.options, field) != getfield(experiences[i].options, field)
                    #println("index $ind is false with field = $field: it needs $(getfield(experiences[i].options, field)) and it has $(getfield(exp.options, field))")
                    is_comparable = false
                    break
                end
            end
        end
        if is_comparable && exp.outputs.solution != [[]] && exp.outputs.delta_eq != 1e12
            pos = findall(x->x==getfield(exp.options, option), option_values)[1]
            comparable_instances[pos] = ind
        end
    end

    #println(option_values)
    #[println(i," ",experiences[comparable_instances[i]].options) for i in 1:length(comparable_instances)]

    return comparable_instances
end

function compare_cs_experience(experiences, option, option_values, filename = "")
    # compute statistics on experiences separately for option_values of option
    # experiences is a list of cs_experience
    # option is a symbol (example: :filename_instance) in the fields of option_cs_instance
    # option_values is a list in the values of field option used in experiences

    n = length(option_values)

    statistics = []
    solveds = [0 for i in 1:n] # count the number of instances solved by option_value
    count = [0 for i in 1:n] # count the number of instances comparable by option_value
    cpu_times = [[] for i in 1:n] # list the cpu_time by option_value
    iters = [[] for i in 1:n] # list the iteration needed by option_value

    for i in 1:length(experiences)
        exp = experiences[i]
        if getfield(exp.options, option) == option_values[1]
            # indices is a set of indices corresponding to experiences comparable to exp, of size length(option_values) because all comparable experiences are found
            indices = find_comparable_experiences(i, experiences, option, option_values)
            #println("indices for $option $option_values:\n$indices")

            if length(indices) != length(option_values) && length(indices) != 0
                error("indices $indices has a length different from option_values")
            end

            # add stats
            #println(indices)
            if !("UNK" in indices)
                for i in 1:length(indices)
                    index = indices[i]
                    solveds[i] += experiences[index].outputs.solved
                    count[i] += 1
                    if experiences[index].options.refinement_method == "SGM_SOCP_model" || experiences[index].options.refinement_method == "SGM_gurobiNL_model"
                        cpu_time = experiences[index].outputs.SGM_time
                    else
                        cpu_time = experiences[index].outputs.SGM_time + experiences[index].outputs.julia_time
                    end
                    push!(cpu_times[i], cpu_time)
                    push!(iters[i], experiences[index].outputs.iter)
                end
            end
        end
    end

    # print important informations
    println()
    s = string("\t\t\t",option_values[1])
    [s = string(s, " ", option_values[i]) for i in 2:length(option_values)]
    println(s)
    #println("instances solved:\t$solveds for $count instances comparable")
    print("instances solved:\t")
    [print(solveds[i], "\t") for i in 1:n]
    println()
    print("instances comparable:\t")
    [print(count[i], "\t") for i in 1:n]
    println()
    if minimum(count) > 0
        print("average cpu times:\t")
        #[print(file, round(sum(cpu_times[i][j] for j in 1:length(cpu_times[i]))/length(cpu_times[i]), digits=2), "\t") for i in 1:n]
        for i in 1:n
            average_time = 0
            cpt = 0
            for j in 1:length(cpu_times[i])
                if cpu_times[i][j] != Inf && cpu_times[i][j] > 0
                    average_time += cpu_times[i][j]
                    cpt += 1
                end
            end
            if cpt > 0
                average_time = round(average_time/cpt, digits=2)
            else
                average_time = "undef" # 0 instances solved, thus the average time is undefined
            end
            print(average_time, "\t")
        end
        println()
        print("average iterations:\t")
        #[print(file, round(sum(iters[i][j] for j in 1:length(iters[i]))/length(iters[i]),digits=2), "\t") for i in 1:n]
        for i in 1:n
            average_iter = 0
            cpt = 0
            for j in 1:length(iters[i])
                if iters[i][j] != Inf && iters[i][j] > 0
                    average_iter += iters[i][j]
                    cpt += 1
                end
            end
            if cpt > 0
                average_iter = round(average_iter/cpt, digits=2)
            else
                average_iter = "undef" # 0 instances solved, thus the average number of iteration is undefined
            end
            print(average_iter, "\t")
        end
    end

    # write important informations in file filename
    if filename != ""
        file = open(filename, "a")
        println(file)
        s = string("\t\t\t",option_values[1])
        [s = string(s, " ", option_values[i]) for i in 2:length(option_values)]
        println(file, s)
        #println("instances solved:\t$solveds for $count instances comparable")
        print(file, "instances solved:\t")
        [print(file, solveds[i], "\t") for i in 1:n]
        println(file)
        print(file, "instances comparable:\t")
        [print(file, count[i], "\t") for i in 1:n]
        println(file)
        if minimum(count) > 0
            print(file, "average cpu times:\t")
            #[print(file, round(sum(cpu_times[i][j] for j in 1:length(cpu_times[i]))/length(cpu_times[i]), digits=2), "\t") for i in 1:n]
            for i in 1:n
                average_time = 0
                cpt = 0
                for j in 1:length(cpu_times[i])
                    if cpu_times[i][j] != Inf && cpu_times[i][j] > 0
                        average_time += cpu_times[i][j]
                        cpt += 1
                    end
                end
                if cpt > 0
                    average_time = round(average_time/cpt, digits=2)
                else
                    average_time = "undef" # 0 instances solved, thus the average time is undefined
                end
                print(file, average_time, "\t")
            end
            println(file)
            print(file, "average iterations:\t")
            #[print(file, round(sum(iters[i][j] for j in 1:length(iters[i]))/length(iters[i]),digits=2), "\t") for i in 1:n]
            for i in 1:n
                average_iter = 0
                cpt = 0
                for j in 1:length(iters[i])
                    if iters[i][j] != Inf && iters[i][j] > 0
                        average_iter += iters[i][j]
                        cpt += 1
                    end
                end
                if cpt > 0
                    average_iter = round(average_iter/cpt, digits=2)
                else
                    average_iter = "undef" # 0 instances solved, thus the average number of iteration is undefined
                end
                print(file, average_iter, "\t")
            end
        end
        close(file)
    end

    return solveds, cpu_times, iters, count
end

function count_NL_BR_failed(experiences)
    # return the number of experiences which failed because the NL BR failed
    # test: exp.delta_eq == 1e12

    cpt = 0
    l = []

    for i in 1:length(experiences)
        exp = experiences[i]
        if exp.outputs.delta_eq == 1e12
            cpt += 1
            push!(l,i)
            println("----- experience $i failed because of the NL BR -----")
            println(exp.options)
            println(exp.outputs)
        end
    end

    return cpt, l
end

function display_output_field(experiences, field)
    # print for each experience in experiences the field of outputs field (Symbol, thus put : at the start)

    for i in 1:length(experiences)
        exp = experiences[i]
        println("$i: $(getfield(exp.outputs,field))")
    end
end

function check_error(experiences)
    # count types and numbers of different errors in experiences
    # WARNING: does not work with 1e12 diff (use count_NL_BR_failed for that)
    error_types = []
    count_error = 0
    counts = []
    failed = []
    for i in 1:length(experiences)
        exp = experiences[i]
        sol = exp.outputs.solution
        if !(typeof(sol) <: Vector{}) || sol == [[]]
            println("experience $i finished with error $sol")
            count_error += 1
            push!(failed, i)
            if !(sol in error_types)
                push!(error_types, sol)
                push!(counts, 1)
            end
        end
    end
    println("indices of the $(length(failed)) failed experiences:")
    println(failed)
    println()
    return error_types, count_error
end

function replace_false_exps(exps, exps_comp)
    # replace experiences in exps by the equivalent in exps_comp

    list_of_options = fieldnames(option_cs_instance)

    for i in 1:length(exps_comp)
        exp_comp = exps_comp[i]
        for j in 1:length(exps)
            exp = exps[j]
            is_comparable = true
            for option in list_of_options
                if getfield(exp_comp.options, option) != getfield(exp.options, option)
                    is_comparable = false
                    break
                end
            end
            if is_comparable
                exps[j] = exps_comp[i]
                break
            end
        end
    end
    return exps, exps_comp
end

function find_not_solved(experiences, option, option_values, solved = false)
    # return indices in experiences of experiences with option_value as parameter option
    # which are not solved (which are solved if solved == true)

    l = []

    for i in 1:length(experiences)
        exp = experiences[i]
        #println(getfield(exp.options, option), " : ", option_values)
        if getfield(exp.options, option) in option_values
            if exp.outputs.solved == solved && exp.outputs.delta_eq != 1e12
                println("$i: $(exp.options)\n$(exp.outputs)")
                push!(l, i)
            end
        end
    end
    return l
end

function relaunch_exp(experiences, number, complete_output = false)
    # launch the experience with options of experiences[number]
    options = experiences[number].options
    try
        t = @elapsed cs_instance, Vs, iter, outputs_SGM, output = SGM_PWL_solver(options.filename_instance,err_pwlh=options.err_pwlh,fixed_costs=options.fixed_costs,
        refinement_method=options.refinement_method,max_iter=options.max_iter,rel_gap=options.rel_gap,big_weights_on_NL_part=options.big_weights_on_NL_part)
        output.cpu_time = t
        output.julia_time += t # it was previously equal to -total_python_time
        if !complete_output
            return cs_experience(options,output)
        else
            return cs_instance, Vs, iter, outputs_SGM, cs_experience(options,output)
        end
    catch e
        if !occursin("IPG-and-PWL",pwd())
            cd("../IPG-and-PWL/src")
        end
        if occursin("ProcessExited(10)", string(e)) # SGM finished with TIME LIMIT reached
            output = output_cs_instance(false, ErrorException("ERROR time limit reached in SGM"), [], Inf, -1, [], [], [], -1, -1)
        #elseif occursin("ProcessExited(11)", string(e)) # SGM finished with MAX ITER reached
        elseif occursin("ProcessExited(11)", string(e)) # SGM finished with MAX ITER reached
            output = output_cs_instance(false, ErrorException("ERROR max iter reached in SGM"), [], Inf, -1, [], [], [], -1, -1)
        elseif occursin("ProcessExited(3)", string(e)) # SGM finished with MAX ITER reached
            output = output_cs_instance(false, ErrorException("ERROR time limit reached in NL BR"), [], Inf, -1, [], [], [], -1, -1)
        else
            output = output_cs_instance(false, e, [], Inf, -1, [], [], [], -1, -1)
            #outputs = output_cs_instance(false, infos[7], [[]], [], 0, -1, [], [])  old
        end
        if !complete_output
            return cs_experience(options,output)
        else
            return cs_instance, Vs, iter, outputs_SGM, cs_experience(options,output)
        end
    end
end

function relaunch_failed_exps(experiences)
    # relaunch all failed experiences in experiences
    l = []
    for i in 1:length(experiences)
        exp = experiences[i]
        sol = exp.outputs.solution
        if !(typeof(sol) <: Vector{}) || sol == [[]]
            experiences[i] = relaunch_exp(experiences, i)
        end
    end
    return experiences
end

function relaunch_failed_exps_specific(experiences)
    # relaunch all failed experiences in experiences
    l = []
    for i in 1:length(experiences)
        exp = experiences[i]
        sol = exp.outputs.solution
        if typeof(sol) == ErrorException && occursin("iteration 1", sol.msg)
            experiences[i] = relaunch_exp(experiences, i)
        end
    end
    return experiences
end

function relaunch_failed_exps_specific2(experiences)
    # relaunch all failed experiences in experiences
    l = []
    for i in 1:length(experiences)
        exp = experiences[i]
        sol = exp.outputs.solution
        if exp.options.refinement_method == "full_refinement"
            experiences[i] = relaunch_exp(experiences, i)
        end
    end
    return experiences
end

function relaunch_failed_exps_specific3(experiences)
    # relaunch all failed experiences in experiences
    l = []
    for i in 1:length(experiences)
        exp = experiences[i]
        sol = exp.outputs.solution
        if typeof(sol) == String && occursin("max iter", sol)
            experiences[i] = relaunch_exp(experiences, i)
        end
    end
    return experiences
end

function make_table(row_names, col_names, data; title = "")
    # create a latex table with name of rows row_names, name of columns col_names
    # and values in data (data[i][j] is the value for row i and col j)
    sep1 = " \\hline\n"
    sep2 = " \\\\"
    sep3 = " & "

    # start
    s = "\\begin{table}[]\n\\centering\n\\begin{tabular}{"
    s = string(s, "c||")
    for i in 1:length(col_names)-1
        s = string(s, "c|")
    end
    s = string(s, "c}", "\n")

    # column names
    s = string(s, sep3)
    for col in col_names[1:length(col_names)-1]
        s = string(s, col, sep3)
    end
    s = string(s, col_names[end], sep2, sep1)
    # row names + data
    for i in 1:length(row_names)
        s = string(s, row_names[i], sep3)
        # data of this row
        for j in 1:length(col_names)-1
            s = string(s, data[i][j], sep3)
        end
        s = string(s, data[i][end], sep2, "\n")
    end
    s = string(s, sep1)
    # end
    s_end = "\\end{tabular}\n\\caption{$title}\n\\end{table}"
    s = string(s, s_end)

    # convert "_" into "\_"
    s = replace(s, "_"=>"\\_")
    return s
end

function print_table(row_names, col_names, data)
    # print a table with the same data as in make_table

    s = ""
    for col_name in col_names
        s = string(s, col_name, "\t")
    end
    s = string(s, "\n")
    for i in 1:length(row_names)
        s = string(s, row_names[i], "\t")
        for j in 1:length(data[i])
            s = string(s, data[i][j], "\t")
        end
        s = string(s, "\n")
    end
    println(s)
    return s
end

#row_names = [1,2,3]
#col_names = [1,2]
#data = zeros(3,2)
#println(make_table(row_names, col_names, data))

function preliminary_analysis(exps, err_pwlhs, fixed_costss, refinement_methods, filename = "")
    # uses a number of functions to analyse the data in exps

    error_types, count_error = check_error(exps)
    count_failed, l_failed = count_NL_BR_failed(exps)
    println("\nTotal number of experiences: $(length(exps))\n")
    options = [:err_pwlh, :fixed_costs, :refinement_method]
    options_values = [err_pwlhs, fixed_costss, refinement_methods]
    solvedss = []
    cpu_timess = []
    iterss = []
    countss = []
    for i in 1:length(options)
        solveds, cpu_times, iters, counts = compare_cs_experience(exps, options[i], options_values[i], filename)
        push!(solvedss, solveds)
        push!(cpu_timess, cpu_times)
        push!(iterss, iters)
        push!(countss, counts)
    end

    println("$count_error occured, with errors :")
    [println(error_types[i]) for i in 1:length(error_types)]
    if length(error_types) == 0
        println("no errors")
    end
    println("$count_failed experiences stopped because of a failed resolution of Couenne in julia")
    println("the indices of those experiences are in l_failed:\n$l_failed")

    # write the information in a latex table format to write it in the file
    tables = []
    try
        col_names = ["solved","comparable","time","iterations"]
        for i in 1:length(options)
            s_option = string(options[i])
            #println(s_option)
            s_option = s_option[2:end]
            row_names = []
            for name in options_values[i]
                push!(row_names, name)
            end
            data = []
            for j in 1:length(row_names)
                # compute average time while avoiding undefined times (-1 and Inf)
                #round(sum(cpu_timess[i][j][k] for k in 1:length(cpu_timess[i][j]))/length(cpu_timess[i][j]), digits=2)
                #[print(file, round(sum(cpu_times[i][j] for j in 1:length(cpu_times[i]))/length(cpu_times[i]), digits=2), "\t") for i in 1:n]
                average_time = 0
                cpt = 0
                for k in 1:length(cpu_timess[i][j])
                    if cpu_timess[i][j][k] != Inf && cpu_timess[i][j][k] > 0
                        average_time += cpu_timess[i][j][k]
                        cpt += 1
                    end
                end
                if cpt > 0
                    average_time = round(average_time/cpt, digits=2)
                else
                    average_time = "undef" # 0 instances solved, thus the average time is undefined
                end

                average_iter = 0
                cpt = 0
                for k in 1:length(iterss[i][j])
                    if iterss[i][j][k] != Inf && iterss[i][j][k] > 0
                        average_iter += iterss[i][j][k]
                        cpt += 1
                    end
                end
                if cpt > 0
                    average_iter = round(average_iter/cpt, digits=2)
                else
                    average_iter = "undef" # 0 instances solved, thus the average number of iteration is undefined
                end

                push!(data, Any[Int(solvedss[i][j]), Int(countss[i][j]), average_time,
                average_iter])
            end
            push!(tables, make_table(row_names, col_names, data))
            println(tables[end])
        end
    catch e
        println("could not create table because of ERROR\n$e")
    end

    if filename != ""
        file = open(filename, "a")
        println(file, "\nTotal number of experiences: $(length(exps))\n")
        println(file, "$count_error occured, with errors :")
        [println(file, error_types[i]) for i in 1:length(error_types)]
        if length(error_types) == 0
            println(file, "no errors")
        end
        println(file, "$count_failed experiences stopped because of a failed resolution of Couenne in julia")
        println(file, "the indices of those experiences are in l_failed:\n$l_failed")
        println(file, "\n\n")
        for i in 1:length(tables)
            println(file, tables[i])
            println(file)
        end
        close(file)
    end

    return error_types, count_error, count_failed, l_failed, options, solvedss, cpu_timess, iterss
end

function select_ref_method_preliminary_analysis(number, exps, err_pwlhs, fixed_costss, refinement_methods)
    # return the preliminary_analysis of only the experiences done with the refinement_method number number

    selected_exps = copy([exps[i] for i in number:length(refinement_methods):length(exps)])
    preliminary_analysis(selected_exps, err_pwlhs, fixed_costss,["full_refinement"])
end

function load_and_select_ref_method_preliminary_analysis(filename, number, err_pwlhs, fixed_costss, refinement_methods)
    # return the preliminary_analysis of only the experiences in filename done with the refinement_method number number

    exps = load_all_outputs(filename)
    select_ref_method_preliminary_analysis(number, exps, err_pwlhs, fixed_costss, refinement_methods)
end

mutable struct option_selection
    option::Symbol
    option_value
    bool_keep::Bool
end

function select_preliminary_analysis(selections, exps, err_pwlhs, fixed_costss, refinement_methods)
    # return the preliminary_analysis of only the experiences of exps corresponding with selections (cf option_selection structure)
    # option should be written with a ":" at first because it needs to be a Symbol
    selected_exps = copy(exps)
    for selection in selections
        l = []
        for exp in selected_exps
            if selection.bool_keep
                selected_exps = copy([selected_exps[i] for i in 1:length(selected_exps) if getfield(selected_exps[i].options,selection.option) == selection.option_value])
            else
                selected_exps = copy([selected_exps[i] for i in 1:length(selected_exps) if getfield(selected_exps[i].options,selection.option) != selection.option_value])
            end
        end
    end
    preliminary_analysis(selected_exps, err_pwlhs, fixed_costss,refinement_methods)
    return selected_exps
end

function load_and_select_preliminary_analysis(filename, selections, err_pwlhs, fixed_costss, refinement_methods)
    # return the preliminary_analysis of only the experiences in filename done with the refinement_method number number

    exps = load_all_outputs(filename)
    exps = select_preliminary_analysis(selections, exps, err_pwlhs, fixed_costss, refinement_methods)
    return exps
end

mutable struct characteristic
    option::Symbol
    option_value
end

mutable struct category
    l_option::Vector{characteristic}
    name::String
end

function method_comparison(exps, list_categories, title = "table"; filename_save = "", time_limit = 100)
    # compute number of solved instances and mean computation time for categories in list_categories

    # keep track of informations
    mean_times = []
    computeds = []
    solveds = []
    iterss = []
    data = []
    row_names = []
    col_names = ["methods", "%solved", "#inst", "time", "iter"]


    for category in list_categories
        println(category)
        mean_time = 0
        computed = 0
        solved = 0
        iters = 0
        for exp in exps
            in_category = true
            # does it meet the required characteristics?
            for characteristic in category.l_option
                if getfield(exp.options, characteristic.option) != characteristic.option_value
                    in_category = false
                    break
                end
            end
            # if yes, add informations
            if in_category
                computed += 1
                if exp.options.refinement_method == "SGM_NL_model" || exp.options.refinement_method == "SGM_SOCP_model" || exp.options.refinement_method == "SGM_gurobiNL_model"
                    cpu_time = exp.outputs.SGM_time
                else
                    cpu_time = exp.outputs.SGM_time + exp.outputs.julia_time
                end
                if exp.outputs.solved && cpu_time <= time_limit
                    solved += 1
                    mean_time += cpu_time
                    iters += exp.outputs.iter
                end
            end
        end
        # add results
        if solved > 0
            push!(mean_times, round(mean_time/solved, digits=2))
            push!(computeds, computed)
            push!(solveds, solved)
            push!(iterss, round(iters/solved, digits=2))
        else
            push!(mean_times, "undef")
            push!(computeds, computed)
            push!(solveds, solved)
            push!(iterss, "undef")
        end
        if computeds[end] != 0
            push!(data, Any[round(solveds[end]/computeds[end], digits=3), computeds[end], mean_times[end], iterss[end]])
        else
            push!(data, Any[0, 0, mean_times[end], iterss[end]])
        end
        push!(row_names, category.name)
    end

    # print results
    s = print_table(row_names, col_names, data)
    println(make_table(row_names, col_names, data; title = title))
    if filename_save != ""
        file = open(filename_save, "a")
        println(file, s)
        println(file)
        println(file, make_table(row_names, col_names, data; title = title))
        close(file)
    end

    return mean_times, computeds, solveds, iterss
end

function launch_method_comparison(filename, exps, refinement_methods, errs, title = ""; filename_save = "")
    # create list_categories for analyse_performance by putting results :
    # for NL versions, only one category
    # for PWL versions, one category per error in errs

    if filename != ""
        exps = load_all_outputs(filename)
    end

    list_categories = []
    for refinement_method in refinement_methods
        if refinement_method[1:4] == "SGM_"
            # it is a NL version, adding only one
            cat = category([characteristic(:refinement_method,refinement_method)], split(refinement_method, "_")[2])
            push!(list_categories, cat)
        else
            for err in errs
                charac1 = characteristic(:refinement_method, refinement_method)
                charac2 = characteristic(:err_pwlh, err)
                name = string(refinement_method, "_", err.delta)
                cat = category([charac1,charac2], name)
                push!(list_categories, cat)
            end
        end
    end
    method_comparison(exps, list_categories, title; filename_save = filename_save)
    return exps
end

struct Profile # informations on one curve of a performance profile
    x # list of values for axis x
    y # list of valeus for axis y
    name # name of the profile (method name)
    dict_options # In case I forgot some things
end

struct Performance_profile
    profiles::Vector{Profile}
    title::String
    x_axis::String
    y_axis::String
    x_range::Vector{Float64} # range of values of x
    y_range::Vector{Float64} # range of values of y
    dict_options # In case I forgot some things
end

function performance_profile(profiles; xlog = false)
    # plot a performance profile of the profiles in profiles
    # display a title, an x_axis, a y_axis...

    # general informations
    p = plot(legend=:bottomright)
    title!(p,profiles.title)
    xlabel!(p,profiles.x_axis)
    ylabel!(p,profiles.y_axis)
    xlims!(p,profiles.x_range[1], profiles.x_range[2])
    ylims!(p,profiles.y_range[1], profiles.y_range[2])
    #=lc for linecolor
    lw for linewidth
    mc for markercolor
    ms for markersize
    ma for markeralpha=#

    # add profiles
    for profile in profiles.profiles
        if xlog
            plot!(p,profile.x, profile.y, label = profile.name, xaxis=:log)
        else
            plot!(p,profile.x, profile.y, label = profile.name)
        end
    end
    return p
end

function prepare_performance_profile_cybersecurity(filename, filename_save = "performance_profile.png", list_categories = []; refinement_methods = ["full_refinement","SGM_SOCP_model"],
    errs = [Absolute(0.5),Absolute(0.05),Absolute(0.005),Absolute(0.0005)], time_limit=900)
    # prepare profiles for a call to function performance_profile
    # the performace profile will be:
    # computation time in x
    # #solved in y

    exps = load_all_outputs(filename)

    # create list_categories if not given
    if list_categories == []
        for refinement_method in refinement_methods
            if refinement_method[1:4] == "SGM_"
                # it is a NL version, adding only one
                cat = category([characteristic(:refinement_method,refinement_method)], split(refinement_method, "_")[2])
                push!(list_categories, cat)
            else
                for err in errs
                    charac1 = characteristic(:refinement_method, refinement_method)
                    charac2 = characteristic(:err_pwlh, err)
                    if refinement_method != "sufficient_refinement"
                        name = string(refinement_method, " ", err.delta)
                    else
                        name = refinement_method
                    end
                    cat = category([charac1,charac2], name)
                    push!(list_categories, cat)
                end
            end
        end
    end

    # last_solved keep the max time necessary to solve all instances of all experiences, to adjust the time shown in the plot
    last_solved = 0

    # find all experiences for each category
    exps_by_category = [[] for i in 1:length(list_categories)]
    for i in 1:length(list_categories)
        category = list_categories[i]
        println(category)
        for exp in exps
            in_category = true
            # does it meet the required characteristics?
            for characteristic in category.l_option
                if getfield(exp.options, characteristic.option) != characteristic.option_value
                    #println("exp does not match category $category: $exp")
                    in_category = false
                    break
                end
            end
            # if yes, add informations
            if in_category
                #println("exp match for category $category: $exp")
                cpu_time = Inf
                if exp.outputs.solved
                    if exp.options.refinement_method == "SGM_NL_model" || exp.options.refinement_method == "SGM_SOCP_model" || exp.options.refinement_method == "SGM_gurobiNL_model"
                        cpu_time = exp.outputs.SGM_time
                    else
                        cpu_time = exp.outputs.SGM_time + exp.outputs.julia_time
                    end
                end
                push!(exps_by_category[i], cpu_time)
            end
        end
        sort!(exps_by_category[i])
        println("Sorted times for $category:\n$(exps_by_category[i])")
        # update last_solved
        if length(exps_by_category[i]) > 0
            last_solved = max(exps_by_category[i][end],last_solved)
        end
    end

    # create profiles
    l_profiles = []
    for i in 1:length(list_categories)
        x = []
        y = []
        n_exps = length(exps_by_category[i])
        println("number of exps for category $(list_categories[i]) is $n_exps")
        if n_exps > 0
            for j in 1:n_exps
                if exps_by_category[i][j] <= time_limit
                    push!(x, exps_by_category[i][j])
                    push!(y, (j-1)/n_exps)
                    push!(x, exps_by_category[i][j])
                    push!(y, j/n_exps)
                end
            end
            # add last point with same fraction of instances solved and time to time_limit to finish the curve in the plot
            if length(y) != 0
                push!(x, time_limit)
                push!(y, y[end])
            else
                push!(x, Inf)
                push!(y, 0)
                push!(x, time_limit)
                push!(y, 0)
            end
            println("adding point ($time_limit,$(y[end-1]))")

            name = list_categories[i].name
            # change some names to fit my slides: "full_refinement"=>"PWL-ANE", "SOCP"=>"SGM-MOSEK"
            #name = replace(name, "full_refinement"=>"PWL-ANE")
            name = replace(name, "full_refinement"=>"2-level approximation")
            name = replace(name, "SOCP"=>"SGM-MOSEK")
            name = replace(name, "gurobiNL"=>"SGM-gurobiQP")
            name = replace(name, "sufficient_refinement"=>"direct approximation")
            push!(l_profiles, Profile(x,y,name,Dict()))
        end
    end

    println("list_categories of length $(length(list_categories)):\n$list_categories")
    #=println("l_profiles:")
    for i in 1:length(list_categories)
        println(l_profiles[i].name)
        println(l_profiles[i].x)
        println(l_profiles[i].y)
    end=#

    # add fields of l_profiles
    title = "Performance profile for cybersecurity game methods"
    x_axis = "seconds"
    y_axis = "proportion of instances solved"
    min_time = minimum([l_profiles[i].x[1] for i in 1:length(l_profiles)])
    #max_time = maximum([l_profiles[i].x[end-1] for i in 1:length(l_profiles)])

    # correct the categories without experiences for a normal plot
    for i in 1:length(l_profiles)
        if l_profiles[i].x[1] == Inf # special code for this case
            l_profiles[i].x[1] = min_time
        end
    end

    #x_range = [min_time,max_time]
    x_range = [min_time,min(time_limit,last_solved)]
    println("\n\nlast solved instance needed $last_solved seconds\n\n")
    y_range = [0,1.0]
    dict_options = Dict()
    #profiles = Performance_profile(l_profiles, title, x_axis, y_axis, x_range, y_range, dict_options)
    profiles = Performance_profile(l_profiles, "", x_axis, y_axis, x_range, y_range, dict_options)

    #return profiles

    # launch Performance_profile
    p = performance_profile(profiles, xlog=true)

    # save plot to filename_save
    savefig(filename_save)

    display(p)
end

function prepare_real_performance_profile_cybersecurity(filename, filename_save = "performance_profile.png", list_categories = []; refinement_methods = ["full_refinement","SGM_SOCP_model"],
    errs = [Absolute(0.5),Absolute(0.05),Absolute(0.005),Absolute(0.0005)], time_limit=900)
    # prepare profiles for a call to function performance_profile
    # the performace profile will be:
    # computation time in x
    # #solved in y

    exps = load_all_outputs(filename)

    # create list_categories if not given
    if list_categories == []
        for refinement_method in refinement_methods
            if refinement_method[1:4] == "SGM_"
                # it is a NL version, adding only one
                cat = category([characteristic(:refinement_method,refinement_method)], split(refinement_method, "_")[2])
                push!(list_categories, cat)
            else
                for err in errs
                    charac1 = characteristic(:refinement_method, refinement_method)
                    charac2 = characteristic(:err_pwlh, err)
                    if refinement_method != "sufficient_refinement"
                        name = string(refinement_method, " ", err.delta)
                    else
                        name = refinement_method
                    end
                    cat = category([charac1,charac2], name)
                    push!(list_categories, cat)
                end
            end
        end
    end

    # find all experiences for each category
    exps_by_category = [[] for i in 1:length(list_categories)]
    for i in 1:length(list_categories)
        category = list_categories[i]
        println(category)
        for exp in exps
            in_category = true
            # does it meet the required characteristics?
            for characteristic in category.l_option
                if getfield(exp.options, characteristic.option) != characteristic.option_value
                    #println("exp does not match category $category: $exp")
                    in_category = false
                    break
                end
            end
            # if yes, add informations
            if in_category
                #println("exp match for category $category: $exp")
                cpu_time = Inf
                if exp.outputs.solved
                    if exp.options.refinement_method == "SGM_NL_model" || exp.options.refinement_method == "SGM_SOCP_model" || exp.options.refinement_method == "SGM_gurobiNL_model"
                        cpu_time = exp.outputs.SGM_time
                    else
                        cpu_time = exp.outputs.SGM_time + exp.outputs.julia_time
                    end
                end
                push!(exps_by_category[i], cpu_time)
            end
        end
    end

    # delete empty categories
    new_categories = []
    new_exps_by_category = []
    for i in 1:length(list_categories)
        if length(exps_by_category[i]) > 0
            println("size of category $i: $(length(exps_by_category[i]))")
            push!(new_categories, list_categories[i])
            push!(new_exps_by_category, exps_by_category[i])
        end
    end
    list_categories = deepcopy(new_categories)
    exps_by_category = deepcopy(new_exps_by_category)

    # check that all remaining categories have the same number of exps
    n_exps = length(exps_by_category[1])
    println("number of exps by solver: ", n_exps)
    for i in 2:length(list_categories)
        if length(exps_by_category[i]) != n_exps
            error("all categories don't have the same number of exps: ", [length(exps_by_categories[j]) for j in 1:length(list_categories)])
        end
    end

    #return exps_by_category,list_categories

    # divide times by best time among solvers for each instance
    for j in 1:n_exps
        min_time = Inf
        for i in 1:length(list_categories)
            #print("$j-$i ")
            #print(min_time)
            #print(exps_by_category[i][j])
            min_time = min(min_time,exps_by_category[i][j])
            if exps_by_category[i][j] == NaN || min_time == NaN
                println("\n\n\n")
                println("$j-$i ")
                println(min_time)
                println(exps_by_category[i][j])
            end
        end
        if min_time != Inf
            for i in 1:length(list_categories)
                exps_by_category[i][j] /= min_time
            end
        end
    end

    # sort after ratios have been computed instead of times
    # last_solved keep the max time necessary to solve all instances of all experiences, to adjust the time shown in the plot
    last_solved = 0
    for i in 1:length(list_categories)
        sort!(exps_by_category[i])
        println("Sorted ratios for $category:\n$(exps_by_category[i])")
        if length(exps_by_category[i]) > 0
            println("max between $(exps_by_category[i][end]) and $last_solved")
            last_solved = max(maximum([exps_by_category[i][j] for j in 1:n_exps if exps_by_category[i][j] != Inf]),last_solved)
        end
    end


    # create profiles
    l_profiles = []
    for i in 1:length(list_categories)
        x = []
        y = []
        n_exps = length(exps_by_category[i])
        println("number of exps for category $(list_categories[i]) is $n_exps")
        if n_exps > 0
            for j in 1:n_exps
                if exps_by_category[i][j] <= time_limit
                    push!(x, exps_by_category[i][j])
                    push!(y, (j-1)/n_exps)
                    push!(x, exps_by_category[i][j])
                    push!(y, j/n_exps)
                end
            end
            # add last point with same fraction of instances solved and time to time_limit to finish the curve in the plot
            if length(y) != 0
                push!(x, time_limit)
                push!(y, y[end])
            else
                push!(x, Inf)
                push!(y, 0)
                push!(x, time_limit)
                push!(y, 0)
            end
            println("adding point ($time_limit,$(y[end-1]))")

            name = list_categories[i].name
            # change some names to fit my slides: "full_refinement"=>"PWL-ANE", "SOCP"=>"SGM-MOSEK"
            #name = replace(name, "full_refinement"=>"PWL-ANE")
            name = replace(name, "full_refinement"=>"2-level approximation")
            name = replace(name, "SOCP"=>"SGM-MOSEK")
            name = replace(name, "gurobiNL"=>"SGM-gurobiQP")
            name = replace(name, "sufficient_refinement"=>"direct approximation")
            push!(l_profiles, Profile(x,y,name,Dict()))
        end
    end

    println("list_categories of length $(length(list_categories)):\n$list_categories")
    #=println("l_profiles:")
    for i in 1:length(list_categories)
        println(l_profiles[i].name)
        println(l_profiles[i].x)
        println(l_profiles[i].y)
    end=#

    # add fields of l_profiles
    title = "Performance profile for cybersecurity game methods"
    x_axis = L"\tau"
    y_axis = L"P\,(r_{p,s} \leq \tau)"
    min_time = minimum([l_profiles[i].x[1] for i in 1:length(l_profiles)])
    #max_time = maximum([l_profiles[i].x[end-1] for i in 1:length(l_profiles)])

    # correct the categories without experiences for a normal plot
    for i in 1:length(l_profiles)
        if l_profiles[i].x[1] == Inf # special code for this case
            l_profiles[i].x[1] = min_time
        end
    end

    #x_range = [min_time,max_time]
    x_range = [min_time,min(time_limit,last_solved)]
    println("\nlast solved instance had a performance ratio of $last_solved")
    y_range = [0,1.0]
    dict_options = Dict()
    #profiles = Performance_profile(l_profiles, title, x_axis, y_axis, x_range, y_range, dict_options)
    profiles = Performance_profile(l_profiles, "", x_axis, y_axis, x_range, y_range, dict_options)

    #return profiles

    # launch Performance_profile
    p = performance_profile(profiles, xlog=true)

    # save plot to filename_save
    savefig(filename_save)

    display(p)
end