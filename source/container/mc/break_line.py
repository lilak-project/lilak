def break_line(lines,print_title=True):
    """
    Break input line into
    * method:    type, name, argument, const, (init/content), comments
    * parameter: type, name, init
    return True(method)/False(parameter), type, name, argument, True(const)/False() init, content, comments
    """
    ################################################### precomment
    comment_list = []
    line_inprocess = lines
    ic1 = line_inprocess.find("//")
    # comment_type
    # 0: 
    # 1: //
    # 2: ///
    # 3: ///<
    # 4: ///<!
    # 5: //!
    comment_type = 0
    while ic1>=0:
        if ic1==line_inprocess.find("///<!"):
            func_linec = line_inprocess[ic1+5:]
            false_persistency = True
            comment_type = 4
        elif ic1==line_inprocess.find("//!"):
            func_linec = line_inprocess[ic1+3:]
            false_persistency = True
            comment_type = 5
        elif ic1==line_inprocess.find("///<"):
            func_linec = line_inprocess[ic1+4:]
            comment_type = 3
        elif ic1==line_inprocess.find("///"):
            func_linec = line_inprocess[ic1+3:]
            comment_type = 2
        else:
            func_linec = line_inprocess[ic1+2:]
            comment_type = 1
        line_inprocess = line_inprocess[:ic1]
        comment_list.append(func_linec)
        ic1 = line_inprocess.find("//")

    ###################################################
    icb1 = line_inprocess.find("{")
    line_before_cb = line_inprocess
    line_after_cb  = ""
    if icb1>0:
        line_before_cb = line_inprocess[:icb1].strip()
        line_after_cb  = line_inprocess[icb1:].strip()

    ################################################### before_cb
    ib1 = line_before_cb.find("(")
    ieq = line_before_cb.find("=")

    func_type = ""
    func_name = ""
    func_init = ""
    func_arguments = ""
    func_const = ""

    def_parameter = False
    def_method = False
    if ib1<0 or (ieq>0 and ieq<ib1):
        ################################################### def_parameter
        def_parameter = True
        if ieq>0: #par without init
            func_type_name, func_init = line_inprocess[:ieq].strip(), line_inprocess[ieq+1:].strip()
        else: #par with init
            func_type_name = line_inprocess.strip()
            func_init = ""

        icomma = func_type_name.find(",")
        ispace = func_type_name.rfind(" ")
        if ispace<0:
            print("ERROR1 configuring type and name: ", lines)
            return False
        if icomma>0:
            while func_type_name[icomma-1]==' ':
                icomma = icomma-1
            ispace = func_type_name[:icomma].rfind(" ")
        func_type, func_names = func_type_name[:ispace].strip(), func_type_name[ispace:].strip()

        if func_names.find(",")>0:
            for par in func_names.split(","):
                func_name = func_name + ", " + par.strip()
            func_name = func_name[2:]
        else:
            func_name = func_names

    else:
        ################################################### def_method
        def_method = True
        line_before_rb, line_after_rb = line_before_cb[:ib1].strip(), line_before_cb[ib1:].strip()

        func_arguments = line_after_rb
        ib2 = func_arguments.rfind(")")
        if ib2<ieq:
            func_arguments, func_init = line_after_rb[:ieq].strip(), line_after_rb[ieq:].strip()

        if ib2>0:
            func_arguments, func_x_par = func_arguments[1:ib2].strip(), func_arguments[ib2:].strip()
            #print(func_arguments, " ################ ", func_x_par)
            if func_x_par.find("const")>=0:
                func_const = "const"
        elif ib2==0:
            func_arguments = ""
        elif ib2<0:
            print("ERROR3 configuring parmeters no ')': ", lines)
            return False

        ispace = line_before_rb.rfind(" ")
        if ispace<0:
            func_type, func_name = "", line_before_rb
        else:
            func_type, func_name = line_before_rb[:ispace].strip(), line_before_rb[ispace:].strip()

    ################################################### configure
    if len(func_name)>0:
        if func_name[len(func_name)-1]==';':
            func_name = func_name[:len(func_name)-1].strip()

    if len(func_init)>0:
        if func_init[len(func_init)-1]==';':
            func_init = func_init[:len(func_init)-1].strip()

    if func_name[0]=='*':
        func_name = func_name[1:]
        func_type = func_type + "*"

    ################################################### after_cb
    if def_method:
        ib3 = line_after_cb.find("{")
        ib4 = line_after_cb.find("}")
        if ib3>=0:
            if ib3+1==ib4:
                func_content = ";"
            else:
                func_content = line_after_cb[ib3+1:ib4].strip()
                if func_content[0]=="\n": func_content = func_content[1:]
                if func_content[len(func_content)-1]=="\n": func_content = func_content[:len(func_content)-1]
        else:
            func_content = ""
    else:
        func_content = ""

    func_comments = '\n'.join(comment_list)

    #return (True if def_method else False), func_type, func_name, func_arguments, func_const, func_init, func_content, func_comments

    print()
    print(">>", lines)
    if def_method:
        print(f"MM /// {func_comments}")
        print(f"MM {func_type} {func_name}({func_arguments}) {func_const} " + "{ " + f"{func_content}" + " }")
    if def_parameter:
        print(f"PP /// {func_comments}")
        print(f"PP {func_type} {func_name} = {func_init};")#" {func_const} " + "{ " + f"{func_content}" + " }")
    #if def_parameter: print("PPPP   ", lines)
    #else            : print("MMMM   ", lines)
    #if print_title:
    #    tt0 = "<type>"
    #    tt1 = "<name>"
    #    tt2 = "<param>"
    #    tt3 = "<content>"
    #    tt4 = "<init>"
    #    tt5 = "<is_const>"
    #    tt6 = "<comment>"
    #    print(f"{tt0:25s}|{tt1:20s}|{tt2:20s}|{tt3:20s}|{tt4:15s}|{tt5:6s}|{tt6}")
    #print(f"{func_type:15s}|{func_name:20s}|{func_arguments:20s}|{func_content:20s}|{func_init:15s}|{str(func_const):6s}|{func_comments}")

def print_method(func_type, func_name, func_arguments, func_const, func_init, func_content, func_comments):
    if len(func_init)!=0:
        func_content = "= "+func_init+";"
        


if  __name__== "__main__":
    break_line("TVector3 fPositionHead;")
    break_line("TVector3 fPositionHead = TVecotor3(-9.9,-9.9,-9.9);")
    break_line("const double a= 1000;")
    break_line("const double a =1000;")
    break_line("const double a   =   1000 ;")
    break_line("void SetPositionTail(double x, double y, double z);")
    break_line("void SetPositionTail(double x, double y=10, double z=11);")
    break_line("void SetPositionTail(TVector3 x=TVector3(1,2,3), double y=10, double z=11);")
    break_line("void SetPositionTail(double x, double y, double z) {}")
    break_line("void SetPositionTail(double x, double y, double z) { fPositionTail(x,y,z); }")
    break_line("LKHitArray fHitArray;")
    break_line("LKHitArray fHitArray = nullptr;")
    break_line("LKHitArray fHitArray = nullptr; ///< ...")
    break_line("LKHitArray fHitArray = nullptr; ///<! ...")
    break_line("LKHitArray *GetHitArray();")
    break_line("LKHitArray *GetHitArray() { return &fHitArray; }")
    break_line("LKHitArray* GetHitArray() { return &fHitArray; }")
    break_line("LKHitArray GetHitArray() const { return fHitArray; }")
    break_line("TGraphErrors *fTrajectoryOnPlane = nullptr; ///<! Graph object for drawing trajectory on 2D event display")
    break_line("TGraphErrors* fTrajectoryOnPlane; ///<! Graph object for drawing trajectory on 2D event display")
    break_line("virtual GetAlphaTail() const;       ///< Alpha at tail (reconstructed back end)")
    break_line("virtual double Energy(int alpha=0)     const = 0; ///< Kinetic energy of track at vertex.")
    break_line("virtual const double Energy(int alpha) const { return 100; } ///< Kinetic energy of track at vertex.")
    break_line("LKClass::LKClass() {}")
    break_line("const double a    ,b, c,d")
    break_line("const double a,   i,b, c,d")
    break_line("const double a,i  ,b, c,d")
    break_line("const double a,  i  ,b, c,d")
    break_line("const double array[] = {0};")
    break_line("const double array[7] = {0,1,2,3,4,5,6};")
