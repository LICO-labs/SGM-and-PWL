using JuMP

function SGM_model_to_csv_auto(player_index, n_players, n_j, Qb_i, max_s_i, c, pwl1d, Q, C, constant_value,
    linear_terms_in_spi, filename, fixed_costs = false, fcost = [], NL_term = "inverse_square_root"; warmstart = [], PWL_general_constraint = false)
    # write in file filename the matrices of the model Nagurney17 in csv format
    # and the 1D PWL function to txt format

    # declare model and common variables and constraints
     model = Model(Gurobi.Optimizer)

     var_Q_i = @variable(model, Q_i[1:n_j] >= 0)
     var_s_i = @variable(model, s_i >= 0) # number n_j+1
     @constraint(model, s_i <= max_s_i)

     # fixed costs to make business with markets or not depending on the value of parameter fixed_costs
     if fixed_costs
         activate_fixed_cost = @variable(model, base_name = "activate_fixed_cost", [j=1:n_j], Bin)
         @constraint(model, [j=1:n_j], Q_i[j] <= Qb_i[j]*activate_fixed_cost[j])
     else
         @constraint(model, [j=1:n_j], Q_i[j] <= Qb_i[j])
     end

     # add variables for approximated terms
     func_h_s_i = @variable(model, h_s_i, lower_bound = 0) # number 2n_j+2 if fixed_costs==true, number n_j+2 if fixed_costs==false

     # write PWL to file so that python code generates the PWL function with the corresponding general constraint of gurobi
     xpts = [] # list of x values
     ypts = [] # list of y values
     for i in 1:length(pwl1d)-1
         push!(xpts, pwl1d[i].xMin)
         push!(ypts, pwl1d[i].fct(xpts[end]))
         if pwl1d[i+1].fct(pwl1d[i+1].xMin) != pwl1d[i].fct(pwl1d[i].xMax) # discontinuous between pieces i and i+1
             push!(xpts, pwl1d[i].xMax)
             push!(ypts, pwl1d[i].fct(pwl1d[i].xMax))
         end
     end
     push!(xpts, pwl1d[end].xMin)
     push!(ypts, pwl1d[end].fct(xpts[end]))
     push!(xpts, pwl1d[end].xMax)
     push!(ypts, pwl1d[end].fct(xpts[end]))
     file = open(filename[1:end-4]*"_PWL_xpts_$player_index.txt", "w")
     for i in 1:length(xpts)
         println(file, xpts[i])
     end
     close(file)
      file = open(filename[1:end-4]*"_PWL_ypts_$player_index.txt", "w")
     for i in 1:length(xpts)
         println(file, ypts[i])
     end
     close(file)

     # add the objective function
     if fixed_costs
         @objective(model, Max, -h_s_i + sum(c[k]*Q_i[k] for k in 1:n_j) + c[end]*s_i - sum(fcost[j]*activate_fixed_cost[j] for j in 1:n_j))
     else
         @objective(model, Max, -h_s_i + sum(c[k]*Q_i[k] for k in 1:n_j) + c[end]*s_i)
     end

     # check validity of model by printing it
     file = open(filename[1:end-4]*"_$player_index.txt", "w")
     println(file, model)
     close(file)

     # extract A and b coefficients, and IntegerIndexes
     # find the list of variables
     ordvar = all_variables(model)
     save_ordvar_to_file(ordvar,filename)
     # define object containing csv_line objects and IntegerIndexes
     l_coefs = []
     r_coefs = []
     IntegerIndexes = []
     # set the constraint counter
     cpt_cons = 0 # starting at 0 and not 1 because lecture in C++ with indices starting at 0
     # loop on the constraints
     list_cons_types = list_of_constraint_types(model)
     for (F,S) in list_cons_types
         if S != MOI.ZeroOne
             cons = all_constraints(model, F, S)
             for k in 1:length(cons)
                 con = constraint_object(cons[k])
                 temp_coefs = []
                 temp_coefs = add_constraint_coefficient_to_A(temp_coefs, ordvar, con, cpt_cons)
                 if S == MOI.LessThan{Float64}
                     # adding coefficients normally
                     for i in 1:length(temp_coefs)
                         push!(l_coefs, deepcopy(temp_coefs[i]))
                     end
                     push!(r_coefs, csv_vector_line(cpt_cons, con.set.upper))
                 elseif S == MOI.GreaterThan{Float64}
                     # adding coefficients times -1
                     for i in 1:length(temp_coefs)
                         t = temp_coefs[i]
                         push!(l_coefs, csv_line(t.row, t.col, -t.value))
                     end
                     push!(r_coefs, csv_vector_line(cpt_cons, -con.set.lower))
                 elseif S == MOI.EqualTo{Float64}
                     # adding coefficients times normally, then times -1
                     #println("adding an EqualTo constraint to positions $cpt_cons and $(cpt_cons+1):")
                     cp_temp_coefs = deepcopy(temp_coefs)
                     #println("LessThan first")
                     for i in 1:length(temp_coefs)
                         t = temp_coefs[i]
                         #println("adding $(t.row) $(t.col) $(t.value)")
                         push!(l_coefs, deepcopy(temp_coefs[i]))
                     end
                     push!(r_coefs, csv_vector_line(cpt_cons, con.set.value))
                     cpt_cons += 1
                     #println("GreaterThan after")
                     for i in 1:length(cp_temp_coefs)
                         t = cp_temp_coefs[i]
                         #println("adding $(t.row+1) $(t.col) $(-t.value)")
                         push!(l_coefs, csv_line(t.row+1, t.col, -t.value))
                     end
                     push!(r_coefs, csv_vector_line(cpt_cons, -con.set.value))
                 end
                 cpt_cons += 1
             end
         else
             # special case for ZeroOne constraints
             cons = all_constraints(model, F, S)
             for k in 1:length(cons)
                 con = constraint_object(cons[k])
                 coef = find_VariableIndex(con.func, ordvar)
                 # first, put the entry in IntegerIndexes
                 push!(IntegerIndexes, coef)
                 # second, add two constraints b>=0 and b<=1
                 push!(l_coefs, csv_line(cpt_cons, coef, -1))
                 push!(r_coefs, csv_vector_line(cpt_cons, 0))
                 cpt_cons += 1
                 push!(l_coefs, csv_line(cpt_cons, coef, 1))
                 push!(r_coefs, csv_vector_line(cpt_cons, 1))
                 cpt_cons += 1
             end
         end
     end

     # extract the objective function
     obj_coefs = []
     obj = objective_function(model)
     obj_coefs = add_objective_coefficient(obj_coefs, ordvar, obj)

     # write in csv format in file filename
     # A in format "row col value"
     file = open(filename[1:end-4]*"_A$player_index.csv", "w")
     for i in 1:length(l_coefs)
         c = l_coefs[i]
         println(file, "$(c.row) $(c.col) $(c.value)")
     end
     close(file)
     # b in format "value"
     file = open(filename[1:end-4]*"_b$player_index.csv", "w")
     for i in 1:length(r_coefs)
         c = r_coefs[i]
         println(file, "$(c.value)") # printing only value because the parser only get that (c.row is just a count so not useful)
         #println(file, "$(c.row) $(c.value)")
     end
     close(file)
     # IntegerIndexes in format "row"
     file = open(filename[1:end-4]*"_IntegerIndexes$player_index.csv", "w")
     for i in 1:length(IntegerIndexes)
         println(file, IntegerIndexes[i])
     end
     close(file)
     # c in format "row value"
     file = open(filename[1:end-4]*"_c$player_index.csv", "w")
     for i in 1:length(obj_coefs)
         c = obj_coefs[i]
         println(file, "$(c.row) $(c.value)") # the standard is maximization
     end
     close(file)

     # write mixed terms C in a file in format "row col value"
     file = open(filename[1:end-4]*"_C$player_index.csv", "w")
     for i in 1:size(C)[1]
         for j in 1:size(C)[2]
             println(file, "$(i-1) $(j-1) $(C[i,j])") # the standard is maximization
         end
     end
     close(file)

     # write quadratic terms Q in a file in format "row col value"
     file = open(filename[1:end-4]*"_Q$player_index.csv", "w")
     for i in 1:size(Q)[1]
         for j in 1:size(Q)[2]
             println(file, "$(i-1) $(j-1) $(-2*Q[i,j])") # the standard is maximization and there is a coefficient -0.5 (cf readme IPG)
         end
     end
     close(file)

     # write linear terms in the other players' variables in format "value"
     file = open(filename[1:end-4]*"_spi_terms$player_index.csv", "w")
     for i in 1:length(linear_terms_in_spi)
         println(file, linear_terms_in_spi[i]) # the standard is maximization
     end
     close(file)

     # write constant term in format "value"
     file = open(filename[1:end-4]*"_constant$player_index.csv", "w")
     println(file, constant_value) # the standard is maximization
     close(file)

     # write size informations in another file
     file = open(filename[1:end-4]*"_sizes$player_index.csv", "w")
     n_var = length(ordvar)
     n_I = n_j*fixed_costs + Int(ceil(log2(length(pwl1d))))
     n_C = n_var - n_I
     n_constr = length(r_coefs)
     println(file, n_var)  # number of columns in A (number of variables)
     println(file, n_constr) # number of rows in A and rows in b (number of constraints)
     println(file, n_I) # number of integer variables
     println(file, n_C) # number of continuous variables
     println(file, n_players) # number of players (size of indices i)
     println(file, n_j) # number of markets (size of indices j)
     println(file, NL_term) # formula of the nonlinear term
     close(file)

     # resolution of the model to check that it is fine
     #println("model: \n\n\n$model")
     # deactivate presolve in case it does not work with PWL
     #=set_optimizer_attribute(model, "Presolve", 0)
     status = JuMP.optimize!(model)
     term_status = JuMP.termination_status(model)
     println("\n\nstatus termination of the model of player $player_index : $term_status\n\n")
     if term_status == MOI.INFEASIBLE
         compute_conflict!(model)
         if MOI.get(model, MOI.ConflictStatus()) == MOI.CONFLICT_FOUND
             iis_model, _ = copy_conflict(model)
             println("conflict :\n")
             print(iis_model)
         end
         error("status MOI.INFEASIBLE detected for player $player_index")
     end=#

     # prepare warm start for the SGM
     if length(warmstart) >= 1 && !PWL_general_constraint
         println("preparing warmstart for false PWL_general_constraint: $PWL_general_constraint")
         # force variables (except PWL variables) to be as in warmstart
         @constraint(model, [i=1:n_j], Q_i[i] == warmstart[i])
         @constraint(model, s_i == warmstart[n_j+1])
         if fixed_costs
             @constraint(model, [i=1:n_j], activate_fixed_cost[i] == warmstart[n_j+1+i])
         end
         # optimize model to find PWL variables values
         status = JuMP.optimize!(model)
         term_status = JuMP.termination_status(model)
         println("\n\nstatus termination of the model of player $player_index : $term_status\n\n")
         if term_status == MOI.INFEASIBLE
             error("warmstart returned infeasible status")
         end
         # find values for each variable
         #println("ordvar :\n$ordvar")
         wm = [JuMP.value(var) for var in ordvar]
         #println(wm)
         println("number of variables in warmstart: $(length(wm))")
         # write them in a file
         file = open(filename[1:end-4]*"_warmstart$player_index.csv", "w")
         for i in 1:length(wm)
             println(file, wm[i])
         end
         close(file)
         # specific return if needed
     elseif length(warmstart) >= 1 && PWL_general_constraint
         println("preparing warmstart for true PWL_general_constraint: $PWL_general_constraint")
         file = open(filename[1:end-4]*"_warmstart$player_index.csv", "w")
         for i in 1:length(warmstart)
             println(file, warmstart[i])
         end
         val_s_i = warmstart[n_j+1]
         for i in 1:length(pwl1d)
             if pwl1d[i].xMin <= val_s_i <= pwl1d[i].xMax
                 println(file, pwl1d[i].fct(val_s_i))
                 break
             end
         end
         close(file)
     else
         # erase everything from file warmstart to say that there is no warmstart
         println("no preparation of warmstart \t\t\t\t\t\tHERE")
         file = open(filename[1:end-4]*"_warmstart$player_index.csv", "w")
         close(file)
     end

     return model, IntegerIndexes, l_coefs, r_coefs, ordvar
end
