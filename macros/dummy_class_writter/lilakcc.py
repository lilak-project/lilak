import os

class lilakcc:
    """
    Write lilak class
    - methods:

        [x] add(self, input_lines)
            [x] set_class(self,line0, acc_spec, method_source)
                [x] set_file_name(self,name)
                [x] set_file_path(self,file_path)
                [x] set_class_comment(self, comment)
                [x] add_inherit_class(self, inherit_acc_class)
                [x] set_tab_size(self, tab_size)
            [x] add_method(self,line, acc_spec, method_source)
            [x] make_header_source(self, line, set_content=""):
            [x] make_method(self, line, tab_no=0, comment="", is_source=False, in_line=False, omit_semicolon=False, set_content=""):
            [x] add_par(self,line, lname, gname, acc_spec, par_setter, par_getter, par_init, par_clear, par_print, par_source)
                [x] make_par(self, par_type, par_name, par_init, par_comments)
            [x] add_input_data_array(self, data_class, data_array_gname, data_array_bname, data_array_lname="", single_data_name="data", input_comment="")
            [x] add_output_data_array(self, data_class, data_array_gname, data_array_bname, data_array_lname="", single_data_name="data", input_comment="", data_array_init_size=0, data_persistency=True):

        [x] break_data_array(self,lines):
        [x] break_line(self,lines)
        [x] check_method_or_par(self,line):
        [x] make_doxygen_comment(self, comment, add_to="", always_mult_line=False, not_for_doxygen=False, is_persistence=True)
        [x] include_headers(self,includes):

        [x] init_print(self):
        [x] print_class(self, mode=0, to_screen=False, to_file=True, print_example_comments=True, includes='LKContainer.h', inheritance=''):
        [x] print_container(self, to_screen=False, to_file=True, print_example_comments=True, includes='', inheritance=''):
        [x] print_task(self, to_screen=False, to_file=True, print_example_comments=True, includes='', inheritance=''):
    """

    def __init__(self, input_lines=''):
        self.name = ""
        self.path = "./"
        self.comment = ""
        self.tab_size = 4
        self.inherit_list = []
        self.data_init_list = []
        self.data_exec_list = []
        self.data_array_def_list = []
        self.par_clear_list = []
        self.par_print_list = []
        self.par_copy_list = []
        self.enum_list = []
        self.par_def_list = [[],[],[]]
        self.par_init_list = []
        self.set_full_list = [[],[],[]]
        self.get_full_list = [[],[],[]]
        self.method_header_list = [[],[],[]]
        self.method_source_list = [[],[],[]]
        self.include_root_list = []
        self.include_lilak_list = []
        self.include_other_list = []
        self.parfile_lines = []
        if len(input_lines)!=0: self.add(input_lines)

###########################################################################################################################################
    def add(self, input_lines):
        """ # Make funciton from line
        Add class, parameter or methods by line
        You may use main keywords to notify the start of the group:
            +path      -- set path to file
            +class     -- add class
            +idata     -- add input data container
            +odata     -- add output data container
            +include   -- add headers to be included
            +private   -- add parameter or method as private
            +protected -- add parameter or method as protected
            +public    -- add parameter or method as public
        for parameter definition using public, protected, private, you can use one of following bullets:
        - '-': just define parameter and noting else
        - '+': define setter, getter
        - '@': define setter, getter and parameter for parameter container

        When defining parameter or method identifying parameter/method will be done automatically.
        Multiline definitions are also allowed.
        ex:
            +include TNothing.h

            +class TEveryting
            +inherit public TSomething
            +inherit public TAnything

            +idata auto fMyData = new TClonesArray("TMyData",100)

            +private double fValue1 = 0.01;
            +private double fValue2 = 0.02;
            +private double fValue3 = 0.03;
            +private double fValue4 = 0.04;

            +public
            double DoSomething(double value1, double value2) {
                fValue += value1;
                fValue -= value2;
                return fValue;
            }

        Sub-keywords exist to support parameter and method definitions.

        ### class sub-keywords:
            +inherit -- inheritance of class ex: public TNamed

        ### input/output data container sub-keywords for 
            +gname  -- set global data container array name
            +lname  -- set local  data container array name
            +bname  -- set parameter name to be registered or registered as branch name
            +pname  -- set local single data container name
            +size   -- set initial size of the data container array
            +persis -- set persistency of branch (True or False). Default is True

        ex:
            +odata auto fMyDataArray = TClonesArray("TMyData",100)
            +lname myDataArray
            +bname firstData
            +pname myData

        ### parameter sub-keywords:
            +gname  -- set global parameter name
            +lname  -- set local  parameter name
            +pname  -- set parameter name to be used in the parameter container
            +persis -- set persistency of parameter (True or False). Default is True
            +setter -- set setter
            +getter -- set getter
            +source -- set content to be included in the Constructor of LKTask
            +init   -- set content to be included in the Init() method of LKTask
            +clear  -- set content to be included in the Clear() method of LKTask
            +print  -- set content to be included in the Print() method of LKTask
            +copy   -- set content to be included in the Copy() method of LKTask
        ex):
            +private TParameter<double> fParameter1 = 0.08;
            +source fParameter1 = new TParameter<double>({pname},-9.99)
            +pname parameter1
            +init fParameter1 -> SetVal({parc} -> GetParString({pname}))
            +clear fParameter1 -> SetVal(-9.99)

        ### method sub-keyword:
            source -- set method to be written in the source
        """

        if len(input_lines)==0:
            print(f"-------- WARNING1! [add()] no input is given!")
            return

        #if input_lines.find("\n")<0:
        #    file_add = open(input_lines, 'r')
        #    input_lines = file_add.read()

        group = []
        group_list = []
        list_line = input_lines.splitlines()
        list_complete = ["path", "class", "idata", "odata", "include", "private", "protected", "public", "enum"]
        list_components = ["comment", "inherit", "gname", "lname", "pname", "bname", "persis", "setter", "getter", "init", "clear", "source", "print", "copy"]

        is_method = True
        head_is_found = False
        for line in list_line:
            if len(line)==0:
                if len(group)>0:
                    group_list.append(group.copy())
                    head_is_found = False
                    group.clear()
            elif line[:2] == '//' or line[:2] == '/*' or line[:2] == ' *' or line[0] == '*' or line.find('+comment')==0:
                group.append(["+","comment",line])
            elif line[0] in ['+','-']:
                ispace = line.find(' ')
                if ispace < 0: ispace = len(line)
                ltype = line[1:ispace] # line type
                content = line[ispace+1:].strip() # line content
                if ltype=='':
                    ltype=='public'
                if ltype in list_complete:
                    if head_is_found == True:
                        if len(group)>0:
                            group_list.append(group.copy())
                            head_is_found = False
                            group.clear()
                    head_is_found = True
                    group.append([line[0],ltype,content])
                elif ltype in list_components:
                    group.append([line[0],ltype,content])
            else:
                group.append([line[0],"",line])
        if len(group)>0:
            group_list.append(group.copy())
            head_is_found = False
            group.clear()

        for group in group_list:

            group_new = []
            for i0 in range(len(group)):
                pm0, ltype0, line0 = group[i0]
                if len(ltype0)==0:
                    continue

                for i1 in range(i0+1,len(group)):
                    pm1, ltype1, line1 = group[i1]
                    if len(ltype1)==0:
                        line0 = line0 + "\n" + line1
                    else:
                        break

                group_new.append([pm0, ltype0,line0])

            for i2 in range(len(group_new)):
                pm0, ltype0, line0 = group_new[i2]
                if ltype0 in list_complete:
                    break

            is_method = self.check_method_or_par(line0)

            if ltype0=='path':
                #print("@@ path")
                self.set_file_path(line0)

            elif ltype0=='class':
                #print("@@ class")
                class_comment = ""
                list_inherit = []
                for pm, ltype, line in group_new:
                    if ltype=='comment': class_comment = line
                    if ltype=='inherit': list_inherit.append(line)
                self.set_class(line0,class_comment=class_comment, list_inherit=list_inherit)

            elif ltype0=='enum':
                #print("@@ enum")
                self.set_enum(line0)

            elif ltype0=='idata':
                #print("@@ idata")
                idata_gname = ""
                idata_lname = ""
                idata_pname = ""
                idata_bname = ""
                idata_comment = ""
                for pm, ltype, line in group_new:
                    method_source = ""
                    if ltype=='gname':   idata_gname = line
                    if ltype=='lname':   idata_lname = line
                    if ltype=='pname':   idata_pname = line
                    if ltype=='bname':   idata_bname = line
                    if ltype=='comment': idata_comment = line
                da_name, da_class, da_size = self.break_data_array(line0)
                if len(idata_gname)==0: idata_gname = da_name
                if len(idata_bname)==0:
                    print(f'-------- WARNING4! "bname" must be set for "idata"')
                    return
                self.add_input_data_array(
                    data_class=da_class,
                    data_array_gname=idata_gname,
                    data_array_bname=idata_bname,
                    data_array_lname=idata_lname,
                    single_data_name=idata_pname,
                    input_comment=idata_comment)

            elif ltype0=='odata':
                #print("@@ odata")
                odata_gname = ""
                odata_lname = ""
                odata_pname = ""
                odata_bname = ""
                odata_comment = ""
                odata_persis = True
                for pm, ltype, line in group_new:
                    if ltype=='gname':  odata_gname = line
                    if ltype=='lname':  odata_lname = line
                    if ltype=='pname':  odata_pname = line
                    if ltype=='bname':  odata_bname = line
                    if ltype=='comment': odata_comment = line
                    if ltype=='persis': odata_persis = (True if (line.strip().lower())=="true" else False)
                da_name, da_class, da_size = self.break_data_array(line0)
                if len(odata_gname)==0: odata_gname = da_name
                if len(odata_bname)==0:
                    print(f'-------- WARNING5! "bname" must be set for "odata"')
                    return
                self.add_output_data_array(
                    data_class=da_class,
                    data_array_gname=odata_gname,
                    data_array_bname=odata_bname,
                    data_array_lname=odata_lname,
                    single_data_name=odata_pname,
                    data_array_init_size=da_size,
                    input_comment=odata_comment,
                    data_persistency=odata_persis)

            elif ltype0=='include':
                #print("@@ include")
                self.include_headers(line)

            elif is_method:
                #print("@@ method")
                acc_spec = ltype0
                method_source = ""
                for pm, ltype, line in group_new:
                    if ltype=='source': method_source = line
                self.add_method(line0, acc_spec=acc_spec, method_source = method_source)

            else:
                #print("@@ par")
                acc_spec = ltype0
                par_gname  = ""
                par_lname  = ""
                par_pname  = ""
                par_persis = True
                par_setter = ""
                par_getter = ""
                par_init   = ""
                par_clear  = ""
                par_print  = ""
                par_copy  = ""
                par_source = ""
                par_comment = ""
                par_parc_pm = pm0
                for pm, ltype, line in group_new:
                    if ltype=='gname':  par_gname  = line
                    if ltype=='lname':  par_lname  = line
                    if ltype=='pname':  par_pname  = line
                    if ltype=='persis': par_persis = (True if (line.strip().lower())=="true" else False)
                    if ltype=='setter': par_setter = line
                    if ltype=='getter': par_getter = line
                    if ltype=='init':   par_init   = line
                    if ltype=='clear':  par_clear  = line
                    if ltype=='print':  par_print  = line
                    if ltype=='copy':   par_copy  = line
                    if ltype=='source': par_source = line
                    if ltype=='comment': par_comment = line
                self.add_par(line0, acc_spec=acc_spec, par_comment=par_comment,
                    gname=par_gname, lname=par_lname,
                    pname=par_pname, par_persis=par_persis, 
                    par_setter=par_setter, par_getter=par_getter,
                    par_init=par_init, par_clear=par_clear,
                    par_print=par_print, par_copy=par_copy, par_source=par_source,
                    par_parc_pm=par_parc_pm)

###########################################################################################################################################
    def set_class(self, line, class_comment="", list_inherit=[]):
        if line.find('class')==0:
            line = line[line.find('class')+5:].strip()

        if line.find("::")>0:
            line = line[line.find('::')+2:].strip()

        if line.find(":")>0:
            after_colon = line[line.find(':')+1:]
            line = line[:line.find(':')]
            if after_colon.find("{")>=0:
                after_colon = after_colon[:after_colon.find('{')]
            list_classes = after_colon.split(",")
            for class_name in list_classes:
                list_inherit.append(class_name)

        if line.find('class')==0:
            class_name = line[5:].strip()
        else:
            class_name = line.strip()

        self.set_file_name(class_name)
        self.set_class_comment(class_comment)

        for inherit_single in list_inherit:
            self.add_inherit_class(inherit_single)
            inherit_single = inherit_single.strip()
            self.include_headers(inherit_single[inherit_single.find(' ')+1:]+".h")
    
###########################################################################################################################################
    def set_tab_size(self, tab_size):
        """Set tab size"""
        self.tab_size = tab_size
        print("++ {:20}: {}".format("Set tab size",tab_size))

    def set_file_name(self,name):
        """Set name of the class"""
        self.name = name
        print("++ {:20}: {}".format("Set class",name))
  
    def set_file_path(self,file_path):
        """Set path where files are created"""
        self.path = file_path
        print("++ {:20}: {}".format("Set file path",file_path))

    def set_class_comment(self, comment):
        """Description of the class"""
        self.comment = comment 
        print("++ {:20}: {}".format("Set class comment",comment))

    def add_inherit_class(self, inherit_acc_class):
        """Description of the class"""
        inherit_acc_class = inherit_acc_class.strip();
        if inherit_acc_class not in self.inherit_list:
            self.inherit_list.append(inherit_acc_class)
            print("++ {:20}: {}".format("Add inherit class",inherit_acc_class))

###########################################################################################################################################
    def set_enum(self, line):
        tab2 = ' '*(self.tab_size*1)*2
        tab3 = ' '*(self.tab_size*1)*3
        list_comp = line.splitlines()
        ename = list_comp[0].strip()
        enum_all = tab2 + f"enum class {ename}\n"
        enum_all = enum_all + tab2 + "{\n"
        for comp in list_comp[1:]:
            ic = comp.find("//")
            if ic<0:
                ic = len(comp)
            cname, comment = comp[:ic].strip(), comp[ic+2:].strip()
            cname = cname + ","
            if len(comment)>0:
                enum_all = enum_all + tab3 + f"{cname:15} // {comment}\n"
            else:
                enum_all = enum_all + tab3 + f"{cname}\n"
        enum_all = enum_all + tab2 + "};\n"
        self.enum_list.append(enum_all)

###########################################################################################################################################
    def add_method(self, line, comment="", acc_spec="public", method_source=""):
        if len(method_source)==0: method_source = line
        method_header = self.make_method(line,         comment=comment)
        method_source = self.make_method(method_source,comment=comment,is_source=True)

        ias = {"public" : 0, "protected": 1, "private" : 2}.get(acc_spec, -1)
        self.method_header_list[ias].append(method_header)
        self.method_source_list[ias].append(method_source)

###########################################################################################################################################
    def make_header_source(self, line, set_content=""):
        if len(set_content)==0:
            set_content = ";"
        header_content = self.make_method(line, tab_no=2, set_content="")
        source_content = self.make_method(line, tab_no=0, is_source=True, set_content=set_content)
        return header_content, source_content

###########################################################################################################################################
    def make_method(self, line, tab_no=0, is_source=False, set_content="", comment="", in_line=False, omit_semicolon=False):
        is_method, method_type, method_name, method_arguments, method_const, method_init, method_contents, method_comments, comment_type = self.break_line(line)
        #if line.find("SetDecayNo")>0:
            #print(is_method, method_type, method_name, method_arguments, method_const, method_init, method_contents, method_comments, comment_type)
            #print("??????",method_contents)
            #print("??????",line)
        if is_source:
            method_name = f"{self.name}::{method_name}"
            method_init = ""
            #if line.find("option")>0: print("4>>>>>",method_arguments)
            method_arguments = method_arguments.replace('=""','')
            #if line.find("option")>0: print("5>>>>>",method_arguments)

        if len(set_content)>0:
            method_contents = set_content

        tab1 = ' '*(self.tab_size*1)

        if omit_semicolon:
            semicolon = ""
        else:
            semicolon = ";"

        line_const = f" const" if len(method_const)>0 else ""
        line_arguments = "(" + method_arguments + ")"
        if len(method_arguments)==0:
            line_arguments = "()"

        if len(method_init)>0:
            line_content = " = " + method_init + semicolon
        elif len(method_contents)>0:
            if len(method_const)>0:
                line_const = line_const + " "
            if in_line:
                line_content = " { " + method_contents + " }"
            elif method_contents.find("\n")>=0 or is_source:
                line_content = "\n{\n"
                for method_content in method_contents.splitlines():
                    line_content = line_content + tab1 + method_content + "\n"
                line_content = line_content + "}"
            else:
                line_content = " { " + method_contents + " }"
        else:
            #line_const = ""
            line_content = semicolon
        if len(method_type)!=0:
            method_type = method_type + " "
        line_final = f"{method_type}{method_name}{line_arguments}{line_const}{line_content}"
        line_final = (" "*self.tab_size)*tab_no + line_final
        line_final = self.make_doxygen_comment(method_comments,line_final)
        return line_final

###########################################################################################################################################
    def add_par(self, lines, 
                acc_spec="public",
                gname="", lname="", pname="", par_persis=True,
                par_setter="", par_getter="",
                par_init="", par_clear="", par_print="", par_copy="",
                par_source="", par_comment="", par_parc_pm=""):
        """ add parameter
        lines               -- Input contents
        acc_spec = "public" -- Access specifier: one of "public", protected", "private"
        gname = ""          -- Global(Field) name used through class. Default : f[lname]
        lname = ""          -- Local name to be used inside the block. 
        pname = ""          -- Parameter name to be used in the parameter container
        par_persis = True   -- Persistency of the parameter (do or do not write in the root file)
        par_setter = ""     -- Contents to be add as Getter.
        par_getter = ""     -- Contents to be add as Setter.
        par_init = ""       -- Contents to be add in the Init() method.
        par_clear = ""      -- Contents to be add in the Clear() method.
        par_print = ""      -- Contents to be add in the Print() method.
        par_copy = ""       -- Contents to be add in the Copy() method.
        par_source = ""     -- Contents to be add in the class constructor.
        """

        is_method, par_type, par_name, par_arguments, par_const, par_initv, par_contents, par_comments, comment_type = self.break_line(lines)
        ias = {"public":0, "protected":1, "private":2}.get(acc_spec, -1)

        gname, lname, pname, dname, mname = self.configure_names(par_name, gname, lname, pname)

        ############ parameter name in parameter container ############
        if len(pname)==0:
            pname = par_name

        pname_comment = ""
        if pname.find('#'):
            pname_comment = pname[pname.find('#')+1:]

        if isinstance(par_initv, str)==False: par_initv = str(par_initv)

        use_par_init = False
        if len(par_init)==0:
            par_file_val = par_initv
        else:
            use_par_init = True
            #pname2 = par_init[:par_init.find(' ')]
            #par_init = par_init.replace("{pname}",pname)
            #if pname2!=pname:
            #    print(f"WARNING3! given pname({pname}) and pname2({pname2}) from par_init, are not same! replacing pname2 to {pname}.")
            #    pname2 = {pname}
            #    par_init = pname + ' ' + par_init[par_init.find(' '):]

        lines0 =           lines
        gname0 =           gname
        lname0 =           lname
        pname0 =           pname
        par_persis0 =      par_persis
        par_setter0 =      par_setter
        par_getter0 =      par_getter
        par_init0 =        par_init
        par_clear0 =       par_clear
        par_print0 =       par_print
        par_copy0 =        par_copy
        par_source0 =      par_source
        par_comment0 =     par_comment

        ############ parameter task init ############

        line_par_comment_in_init = ""
        if   par_type=="bool":        par_type_getpar = "Bool"
        elif par_type=="int":         par_type_getpar = "Int"
        elif par_type=="double":      par_type_getpar = "Double"
        elif par_type=="float":       par_type_getpar = "Double"
        elif par_type=="Bool_t":      par_type_getpar = "Bool"
        elif par_type=="Int_t":       par_type_getpar = "Int"
        elif par_type=="Double_t":    par_type_getpar = "Double"
        elif par_type=="Float_t":     par_type_getpar = "Double"
        elif par_type=="TString":     par_type_getpar = "String"
        elif par_type=="const char*": par_type_getpar = "String"
        elif par_type=="Color_t":     par_type_getpar = "Color"
        elif par_type=="Width_t":     par_type_getpar = "Width"
        elif par_type=="Size_t":      par_type_getpar = "Size"
        elif par_type=="TVector3":
            par_type_getpar = "V3"
            par_file_val = par_file_val[par_file_val.find('(')+1:par_file_val.find(')')]
            par_file_val = par_file_val.replace(',','  ')
        else:
            line_par_comment_in_init = f"//TODO The type {par_type} is not featured with LKParameterContainer. Please modify Below:"
            par_type_getpar = par_type
        line_par_init_in_init = f'{gname} = fPar -> GetPar{par_type_getpar}("{self.name}/{pname}");'

        ############ parameter definition ############
        init_from_header = True
        if use_par_init:
            init_from_header = False
        elif len(par_initv)==0:
            init_from_header = False
        elif par_initv.find('->')>0:
            init_from_header = False
        elif par_initv.find('.')>0:
            if par_initv[par_initv.find('.')+1].isdigit()==False:
                init_from_header = False

        par_type_ = par_type
        if   len(par_type_)<12: par_type_ = f'{par_type_:12}'
        elif len(par_type_)<16: par_type_ = f'{par_type_:16}'
        elif len(par_type_)<20: par_type_ = f'{par_type_:20}'

        if init_from_header: par_def = f'{dname} = {par_initv};'
        else:                par_def = f'{dname};'

        if   len(par_def)<20: par_def = f'{par_def:20}'
        elif len(par_def)<25: par_def = f'{par_def:25}'
        elif len(par_def)<30: par_def = f'{par_def:30}'
        elif len(par_def)<40: par_def = f'{par_def:40}'
        elif len(par_def)<50: par_def = f'{par_def:50}'
        elif len(par_def)<60: par_def = f'{par_def:60}'

        line_par_definition = f'{par_type_} {par_def}'
        line_par_definition = self.make_doxygen_comment(par_comments,line_par_definition,is_persistence=par_persis)

        if use_par_init: line_par_in_par_container = f'{par_init}'
        else:            line_par_in_par_container = f'{self.name}/{pname} {par_file_val}'
        line_par_in_par_container = self.make_doxygen_comment(pname_comment,line_par_in_par_container,comment_type="#")

        ############ parameter clear ############
        if len(par_clear)==0:
            if use_par_init:
                line_par_in_clear = f'{par_initv};';
            elif len(par_initv)>0:
                line_par_in_clear = f'{gname} = {par_initv};';
            else:
                line_par_in_clear = f'{gname};';
        else:
            line_par_in_clear = par_clear

        ############ parameter print ############
        if len(par_print)==0:
            if par_type in ["bool", "int", "double", "float", "Bool_t", "Int_t", "Double_t", "Float_t", "TString", "const char*"]:
                line_par_in_print = f'lx_info << "{par_name} : " << {gname} << std::endl;'
            else:
                line_par_in_print = f'//lx_info << "{par_name} : " << {gname} << std::endl;'
        else:
            line_par_in_print = self.make_method(par_print.replace("{gname}",gname), in_line=True)

        ############ settter definition ############
        if len(par_setter)==0:
            set_type = par_type
            set_name = "Set" + mname[0].title()+mname[1:]
            line_set_par = self.make_method(f"void {set_name}({set_type} {lname}) "+"{ "+f"{gname} = {lname};"+" }", in_line=True)
        else:
            line_set_par = self.make_method(par_setter.replace("{lname}",lname), in_line=True)
            is_method, set_type, set_name, dp, dp, dp, dp, dp, dp = self.break_line(line_set_par)
  
        ############ gettter definition ############
        if len(par_getter)==0:
            get_type = par_type
            get_name = "Get" + mname[0].title()+mname[1:]
            line_get_par = self.make_method(f"{get_type} {get_name}() const " +"{"+ f"return {gname};"+"}", in_line=True)
        else:
            line_get_par = self.make_method(par_getter.replace("{gname}",gname), in_line=True)
  
        ############ parameter copy ############
        if len(par_copy)==0:
            line_par_in_copy = f"objCopy.{set_name}({gname});"
        else:
            line_par_in_copy = self.make_method(par_copy.replace("{gname}",gname), in_line=True)

        ############  ############  ############
        if par_parc_pm=='@':
            if len(line_par_comment_in_init)!=0:
                self.par_init_list.append(line_par_comment_in_init);
            self.par_init_list.append(line_par_init_in_init);
        self.par_clear_list.append(line_par_in_clear);
        self.par_print_list.append(line_par_in_print);
        self.par_copy_list.append(line_par_in_copy);
        self.par_def_list[ias].append(line_par_definition);
        if par_parc_pm in ['+','@']:
            self.set_full_list[0].append(line_set_par);
        if par_parc_pm in ['+','@']:
            self.get_full_list[0].append(line_get_par);
        if par_parc_pm=='@':
            self.parfile_lines.append(line_par_in_par_container)

        #print('++ par    :'  ,lines0)
        #print('   gname  :'  ,gname0, " => ", gname)
        #print('   lname  :'  ,lname0, " => ", lname)
        #print('   pname  :'  ,pname0, " => ", pname)
        #print('   persis :'  ,par_persis0, " => ", par_persis)
        #print('   setter :'  ,par_setter0, " => ", line_set_par)
        #print('   getter :'  ,par_getter0, " => ", line_get_par)
        #print('   init   :'  ,par_init0, " => ", line_par_init_in_init)
        #print('   clear  :'  ,par_clear0, " => ", line_par_in_clear)
        #print('   print  :'  ,par_print0, " => ", line_par_in_print)
        #print('   copy   :'  ,par_copy0, " => ", line_par_in_copy)
        #print('   source :'  ,par_source0, " => ...")
        #print('   comment:'  ,par_comment0)


###########################################################################################################################################
    def make_par(self, line):

        is_method, par_type, par_name, par_arguments, par_const, par_init, par_contents, par_comments, comment_type = self.break_line(lines)
        par_header = self.make_par(par_type=par_type, par_name=par_name, par_init=par_init, par_comments=par_comments)
        ias = {"public":0, "protected":1, "private":2}.get(acc_spec, -1)
        print("==", ias, par_header)

        line_init = f" = {par_init}" if len(par_init)>0 else ""
        line = f"{par_type} {par_name}{line_init};"
        line = self.make_doxygen_comment(par_comments,line)
        return line
        
###########################################################################################################################################
    def check_method_or_par(self,line):
        ic1 = line.find("//")
        #if ic1==0: return False
        line_before_cb = line[:ic1].strip() if line.find("{")>0 else line
        ib1 = line_before_cb.find("(")
        ieq = line_before_cb.find("=")
        if ib1<0 or (ieq>0 and ieq<ib1): return False
        else: return True

###########################################################################################################################################
    def configure_names(self, given_name, gname, lname, pname):
        """
        gname -- global name
        lname -- local name
        pname -- parameter name of parameter container
        dname -- definition name in header (needed for array definition)
        mname -- middle name to be used in the middle of function name. ex) Get[mname]
        return gname, lname, pname, dname
        """

        lname = given_name
        if given_name[0]=="f":
            lname = given_name[1:]
            lname = lname[0].lower()+lname[1:]
            if len(gname)==0:
                if given_name[1].isupper():
                    gname = "f" + lname[0].title()+lname[1:]
                    mname = lname[0].title()+lname[1:]
                elif given_name[1].islower():
                    gname = "f" + lname[0]+lname[1:]
                    mname = lname[0]+lname[1:]

        else:
            lname = given_name
            if len(gname)==0:
                gname = "f" + lname[0]+lname[1:]
                mname = lname[0]+lname[1:]

        dname = gname

        ############ local parameter name ############
        if len(lname)==0:
            lname = given_name
        if lname==gname:
            print(f"-------- WARNING2! gname({gname}) and lname({lname}) are same! replacing lname to {lname}_.")
            lname = lname + "_"

        ib1 = given_name.find("[")
        if ib1>0: given_name = given_name[:ib1]
        ib1 = gname.find("[")
        if ib1>0: gname = gname[:ib1]
        ib1 = lname.find("[")
        if ib1>0: lname = lname[:ib1]

        ############ parameter name in parameter container ############
        if len(pname)==0:
            pname = lname

        return gname, lname, pname, dname, mname

###########################################################################################################################################
    def break_data_array(self,lines):
        pre_and_name, tclonesarray_def = lines[:lines.find("=")].strip(), lines[lines.find("=")+1:].strip()

        ispace = pre_and_name.find(" ")
        if ispace<0: da_name = pre_and_name
        else:        da_name = pre_and_name[ispace:].strip()

        tclonesarray_def = tclonesarray_def[tclonesarray_def.find("(")+1:tclonesarray_def.rfind(")")]
        arguments = tclonesarray_def.split(',')

        d0_class = arguments[0]
        d0_class = d0_class[d0_class.find('"')+1:d0_class.rfind('"')]
        da_size = 0
        if len(arguments)>1:
            da_size = int(arguments[1])

        return da_name, d0_class, da_size

###########################################################################################################################################
    def break_line(self,lines):
        """
        Break input line into
        * method:    type, name, argument, const, (init/content), comments
        * parameter: type, name, init
        return True(method)/False(parameter), type, name, arguments, "const"/"", init, contents, comments, comment_type
        """
        ################################################### precomment
        comment_list = []
        line_inprocess = lines
        ic1 = line_inprocess.find("//")
        ibe = line_inprocess.rfind("}")
        # comment_type
        # 0:
        # 1: //
        # 2: ///
        # 3: ///<
        # 4: ///<!
        # 5: //!
        comment_type = ""
        while ic1>=0 and ic1>ibe:
            if ic1==line_inprocess.find("///<!"):
                func_linec = line_inprocess[ic1+5:]
                false_persistency = True
                comment_type = "///<!"
            elif ic1==line_inprocess.find("//!"):
                func_linec = line_inprocess[ic1+3:]
                false_persistency = True
                comment_type = "//!"
            elif ic1==line_inprocess.find("///<"):
                func_linec = line_inprocess[ic1+4:]
                comment_type = "///<"
            elif ic1==line_inprocess.find("///"):
                func_linec = line_inprocess[ic1+3:]
                comment_type = "///"
            else:
                func_linec = line_inprocess[ic1+2:]
                comment_type = "//"
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
        ib2 = line_before_cb.find(")")
        ieq = line_before_cb.find("=")

        func_type = ""
        func_name = ""
        func_init = ""
        func_arguments = ""
        func_const = ""

        def_parameter = False
        is_method = False
        #if ib1>0 and ieq>0 and b2>0 and b2>ieq:
            ################################################### def_parameter

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
                ispace=0
                #print("------- ERROR1 configuring type and name: ", lines, " -------")
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
            ################################################### is_method
            is_method = True
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
                print("ERROR2 configuring parmeters no ')': ", lines)
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
        if is_method:
            ib3 = line_after_cb.find("{")
            ib4 = line_after_cb.find("}")
            if ib3>=0:
                if ib3+1==ib4:
                    func_contents = ";"
                else:
                    func_contents = line_after_cb[ib3+1:ib4].strip()
                    if len(func_contents)==0: func_contents = ""
                    else:
                        if func_contents[0]=="\n": func_contents = func_contents[1:]
                        if func_contents[len(func_contents)-1]=="\n": func_contents = func_contents[:len(func_contents)-1]
            else:
                func_contents = ""
        else:
            func_contents = ""

        func_comments = '\n'.join(comment_list)

        return (True if is_method else False), func_type, func_name, func_arguments, func_const, func_init, func_contents, func_comments, comment_type

###########################################################################################################################################
    def make_doxygen_comment(self, comment, add_to="", always_mult_line=False, not_for_doxygen=False, is_persistence=True, comment_type=""):
        """Make doxygen comment

        add_to (string) -- If add_to parameter is set True, comment will be put after (before)
                           Return comment, is_mult_line where is_mult_line is True when comment is multi-line if add_to is set False. 
        always_mult_line (bool) -- The comments are assumed that it is multi-line and use /** [...] */
        not_for_doxygen (bool) -- If True: /** -> /*, ///< -> //
        """
        #go_debug = False
        #if comment.find("Raw event")>0:
        #    go_debug = True
        #    print(comment)

        if comment_type.strip()=="#":
            always_mult_line = False

        multi_line_comment = False
        single_line_comment = False
        if always_mult_line or "\n" in comment or "\r\n" in comment:
            multi_line_comment = True
        else:
            single_line_comment = True

        if is_persistence and len(comment)==0:
            return add_to
        else:
            if always_mult_line or "\n" in comment or "\r\n" in comment:
                lines = comment.split('\n')

                for i in range(len(lines)):
                    lines[i] = lines[i].strip()
                    if lines[i].find("///<!")==0:  lines[i] = lines[i][5:]
                    elif lines[i].find("///<")==0: lines[i] = lines[i][4:]
                    elif lines[i].find("//!")==0:  lines[i] = lines[i][3:]
                    elif lines[i].find("///")==0:  lines[i] = lines[i][3:]
                    elif lines[i].find("/**")==0:  lines[i] = lines[i][3:]
                    elif lines[i].find("//")==0:   lines[i] = lines[i][2:]
                    elif lines[i].find("*/")==0:   lines[i] = lines[i][2:]
                    elif lines[i].find("*")==0:    lines[i] = lines[i][1:]
                    lines[i] = lines[i].strip()
                    lines[i] = ' * ' + lines[i]
                if not_for_doxygen: lines.insert(0,"/*")
                else:               lines.insert(0,"/**")
                lines.append(" */")
                comment = '\n'.join(lines)
                return comment + add_to
            else:
                if len(comment_type)==0:
                    if   not_for_doxygen: comment_type = "//"
                    elif is_persistence:  comment_type = "///<"
                    else:                 comment_type = "///<!"
                comment = " " + comment_type + " " + comment;
                return add_to + comment

###########################################################################################################################################
    def include_headers(self,includes):
        """Include header files assuming that includes are separated by spaces or line-break"""
        if len(includes)==0:
            return
        header_list = includes.replace(' ','\n')
        header_list = includes.split('\n')
        for header in header_list:
            if header[:8]!="#include":
                if header[0]=='"' or header[0]=='<':
                    header_full = "#include "+header
                else:
                    header_full = "#include \""+header+"\""
            else:
                header_full = header

            print("++ {:20}: {}".format("include header",header_full))
            if header[0]=="T":
                if header_full not in self.include_root_list:
                    self.include_root_list.append(header_full)
            elif header[:2]=="LK":
                if header_full not in self.include_lilak_list:
                    self.include_lilak_list.append(header_full)
            else:
                if header_full not in self.include_other_list:
                    self.include_other_list.append(header_full)

###########################################################################################################################################
    def add_input_data_array(self, data_class, data_array_gname, data_array_bname, data_array_lname="", single_data_name="data", input_comment=""):
        data_array_gname, data_array_lname, dummy, dummy, dummy = self.configure_names(data_array_gname, data_array_lname, data_array_lname, "")

        if len(single_data_name)==0:
            if data_array_lname.endswith("Array"):
                single_data_name = data_array_lname[:len(data_array_lname)-5]
            else:
                single_data_name = data_array_lname + "_single"

        self.data_array_def_list.append(f"TClonesArray *{data_array_gname} = nullptr;")
        self.data_init_list.append(f'fTrackArray = fRun -> GetBranchA("{data_array_bname}");')

        num_data = "num" + data_array_bname[0].title()+data_array_bname[1:]
        i_data = "i" + data_array_bname[0].title()+data_array_bname[1:]
        tab1 = ' '*(self.tab_size*1)
        data_exec = f"""// Call {single_data_name} from {data_array_gname} and get data value
int {num_data} = {data_array_gname} -> GetEntriesFast();
for (int {i_data} = 0; {i_data} < {num_data}; ++{i_data})"""
        data_exec = data_exec + "\n{"
        data_exec = data_exec + f"""
{tab1}auto *{single_data_name} = ({data_class} *) {data_array_gname} -> At({i_data});
{tab1}//auto value = {single_data_name} -> GetDataValue(); ..."""
        data_exec = data_exec + "\n}"
        self.data_exec_list.append(data_exec)

        print('++ idata')
        print('   data_class       :', data_class)
        print('   data_array_gname :', data_array_gname)
        print('   data_array_bname :', data_array_bname)
        print('   data_array_lname :', data_array_lname)
        print('   single_data_name :', single_data_name)
        print('   num_data         :', num_data)



###########################################################################################################################################
    def add_output_data_array(self, data_class, data_array_gname, data_array_bname, data_array_lname="", single_data_name="data",
                              data_array_init_size=0, input_comment="", data_persistency=True):
        data_array_gname, data_array_lname, dummy, dummy, dummy = self.configure_names(data_array_gname, data_array_lname, data_array_lname, "")

        if len(single_data_name)==0:
            if data_array_lname.endswith("Array"):
                single_data_name = data_array_lname[:len(data_array_lname)-5]
            else:
                single_data_name = data_array_lname + "_single"

        self.data_array_def_list.append(f"TClonesArray *{data_array_gname} = nullptr;")

        if data_array_init_size>0: self.data_init_list.append(f'{data_array_gname} = new TClonesArray("{data_class}",{data_array_init_size});')
        else:                      self.data_init_list.append(f'{data_array_gname} = new TClonesArray("{data_class}");')
        self.data_init_list.append(f'fRun -> RegisterBranch("{data_array_bname}", {data_array_gname});')

        line_par_in_par_container = f'{data_array_bname}/persistency true'
        self.parfile_lines.append(line_par_in_par_container)


        num_data = "num" + data_array_bname[0].title()+data_array_bname[1:]
        i_data = "i" + data_array_bname[0].title()+data_array_bname[1:]
        tab1 = ' '*(self.tab_size*1)
        data_exec = f"""
// Construct (new) {single_data_name} from {data_array_gname} and set data value
for (int {i_data} = 0; {i_data} < {num_data}; ++{i_data})"""
        data_exec = data_exec + "\n{"
        data_exec = data_exec + f"""
{tab1}auto {single_data_name} = ({data_class} *) {data_array_gname} -> ConstructedAt({i_data});
"""
#{tab1}// {single_data_name} -> SetData(value); ...
        data_exec = data_exec + "}"
        self.data_exec_list.append(data_exec)

        print('++ idata')
        print('   data_class       :', data_class)
        print('   data_array_gname :', data_array_gname)
        print('   data_array_bname :', data_array_bname)
        print('   data_array_lname :', data_array_lname)
        print('   single_data_name :', single_data_name)
        print('   num_data         :', num_data)

###########################################################################################################################################
    def init_print(self):
        if os.path.exists(self.path)==False:
            os.mkdir(self.path)

###########################################################################################################################################
    def print_class(self, mode=0, to_screen=False, to_file=True, print_example_comments=True, includes='', inheritance=''):
        """
        ---
        mode -- 0 : print task

        Print header and source file of lilak task class content to screen or file

        to_screen (bool ; False) -- If True, print container to screen
        to_file (bool ; True) -- If True, print container to file ({path}/{name}.cpp, {path}/{name}.h) 
        print_example_comments (bool ; True) -- Print comments that helps you filling up reset of the class.
        includes (string ; '') -- headers to be included separated by space
        includes (string ; '') -- headers to be included separated by space
        inheritance (string ; ' -- Class inheritance.

        ---
        mode -- 1 : print container

        Print header and source file of lilak container class content to screen or file

        to_screen (bool ; False) -- If True, print container to screen
        to_file (bool ; True) -- If True, print container to file ({path}/{name}.cpp, {path}/{name}.h) 
        print_example_comments (bool ; True) -- Print comments that helps you filling up reset of the class.
        includes (string ; '') -- headers to be included separated by space
        inheritance (string ; ' -- Class inheritance.

        ---
        mode -- 2 : print detector

        """
        self.init_print()

        m_task = 0
        m_container = 1
        m_detector = 2
        m_detector_plane = 3

        print(self.inherit_list)
        if len(self.inherit_list)==0:
            if mode==m_task: self.add_inherit_class('public LKTask')
            if mode==m_container: self.add_inherit_class('public LKContainer')
            if mode==m_detector: self.add_inherit_class('public LKDetector')
            if mode==m_detector_plane: self.add_inherit_class('public LKDetectorPlane')

        inheritance = ', '.join(self.inherit_list)

        br1 = "{"
        br2 = "}"

        self.include_headers('LKLogger.h')
        if mode==m_task:
            self.include_headers('LKTask.h')
            self.include_headers('TClonesArray.h')
            self.include_headers('LKTask.h')
            self.include_headers('LKParameterContainer.h')
            self.include_headers('LKRun.h')

        if mode==m_container:
            self.include_headers('TClonesArray.h')
            self.include_headers('LKContainer.h')

        if mode==m_detector:
            self.include_headers('LKDetector.h')

        if mode==m_detector_plane:
            self.include_headers('LKDetectorPlane.h')

        if len(includes)!=0:
            self.include_headers(includes)

        inherit_class_list = inheritance.split(',')
        inherit_class0 = inherit_class_list[0]
        inherit_class0 = inherit_class0.replace('public',' ')
        inherit_class0 = inherit_class0.replace('private',' ')
        inherit_class0 = inherit_class0.replace('private',' ')
        inherit_class0 = inherit_class0.strip()

        tab1 = ' '*(self.tab_size*1)
        tab2 = ' '*(self.tab_size*2)
        etab1 = '\n'+tab1
        etab2 = '\n'+tab2

        name_upper = self.name.upper()
        header_define = f"""#ifndef {name_upper}_HH
#define {name_upper}_HH
"""
        header_detail = ""
        if mode==m_task:
            header_detail = """Remove this comment block after reading it through
    Or use print_example_comments=False option to omit printing

# Example LILAK task class

    - Write Init() method.
    - Write Exec() or/and EndOfRun() method."""
        elif mode==m_container:
            header_detail="""Remove this comment block after reading it through
    Or use print_example_comments=False option to omit printing

# Example LILAK container class

## Must
    - Write Clear() method
      : Clear() method clears and intialize the data class.
        This is "Must Write Method" because containers are not recreated each event,
        but their memories are reused after Clear method is called.
        and they are filled up and written to tree each event.
        See: https://root.cern/doc/master/classTClonesArray.html, https://opentutorials.org/module/2860/19477

    - Version number (2nd par. in ClassDef of source file) should be changed if the class has been modified.
    This notifiy users that the container has been update in the new LILAK (or side project version).

## Recommended
    - Documentaion like this!
    - Write Print() to see what is inside the container;

## If you have time
    - Write Copy() for copying object"""
        elif mode==m_detector:
            header_detail="""Remove this comment block after reading it through
    Or use print_example_comments=False option to omit printing

# Example LILAK detector class
"""

        elif mode==m_detector_plane:
            header_detail="""Remove this comment block after reading it through
    Or use print_example_comments=False option to omit printing

# Example LILAK detector plane class

# Given members in LKDetectorPlane class

## public:
    - void SetDetector(LKDetector *detector);
    - void SetPlaneID(Int_t id);
    - Int_t GetPlaneID() const;
    - void AddChannel(LKChannel *channel);
    - LKChannel *GetChannelFast(Int_t idx);
    - LKChannel *GetChannel(Int_t idx);
    - Int_t GetNChannels();
    - TObjArray *GetChannelArray();
    - LKVector3::Axis GetAxis1();
    - LKVector3::Axis GetAxis2();

## protected:
    - TObjArray *fChannelArray = nullptr;
    - Int_t fPlaneID = -1;
    - TCanvas *fCanvas = nullptr;
    - TH2 *fH2Plane = nullptr;
    - LKVector3::Axis fAxis1 = LKVector3::kX;
    - LKVector3::Axis fAxis2 = LKVector3::kY;
    - LKDetector *fDetector = nullptr;
"""

        if print_example_comments==True:
            header_detail = self.make_doxygen_comment(header_detail,not_for_doxygen=True) + "\n"
            header_detail = header_detail + "\n"
        else:
            header_detail = ""
        header_include_lilak = '\n'.join(sorted(set(self.include_lilak_list)))
        header_include_root = '\n'.join(sorted(set(self.include_root_list)))
        header_include_other = '\n'.join(sorted(set(self.include_other_list)))
        header_description = self.make_doxygen_comment(self.comment,always_mult_line=True)
        header_class = f"class {self.name} : {inheritance}" + "\n{"

        source_include = f'#include "{self.name}.h"'

        ############## public ##############
        self.par_init_list.insert(0,"// Put intialization todos here which are not iterative job though event")
        self.par_init_list.insert(1,f'lx_info << "Initializing {self.name}" << std::endl;')
        self.par_init_list.insert(2,"")
        init_content = '\n'.join(self.par_init_list)

        self.data_init_list.insert(0,"")
        init_content = init_content + '\n'.join(self.data_init_list)
        init_content = init_content + '\n'*2 + 'return true;'

        self.data_exec_list.append(f'lx_info << "{self.name} container" << std::endl;')
        exec_content = '\n'.join(self.data_exec_list)

        self.par_clear_list.insert(0,f"{inherit_class0}::Clear(option);")
        clear_content = '\n'.join(self.par_clear_list)

        self.par_print_list.insert(0,"// You will probability need to modify here")
        self.par_print_list.insert(1,f"{inherit_class0}::Print();")
        self.par_print_list.insert(2,f'lx_info << "{self.name} container" << std::endl;')
        print_content = '\n'.join(self.par_print_list)

        dete2_content = """// example plane
// auto plane = new LKDetectorPlane();
// plane -> SetPlaneID(0);
// plane -> SetDetector(this);
// plane -> Init();
return true;
"""
        findch_content = f"""// example find id
// int id = 100*i + j;
// return id;
return -1;"""

        getcvs_content = f"""// example canvas
// if (fCanvas==nullptr)
//     fCanvas = new TCanvas("{self.name}",{self.name});
// return fCanvas;
return (TCanvas *) nullptr;"""

        gethist_content = f"""// example hist
// if (fHist==nullptr)
//     fHist = new TH2D("{self.name}",{self.name},10,0,10);
// return fHist;
return (TH2D *) nullptr;"""

        self.par_copy_list.insert(0,"// You should copy data from this container to objCopy")
        self.par_copy_list.insert(1,f"{inherit_class0}::Copy(object);")
        self.par_copy_list.insert(2,f"auto objCopy = ({self.name} &) object;")
        copy_content = '\n'.join(self.par_copy_list)

        header_class_public = ' '*self.tab_size + "public:"
        constructor_content = ""
        if mode==m_container: constructor_content = "Clear();"

        header_enum = '\n'+'\n'.join(self.enum_list) if len(self.enum_list)>0 else ""

        ############## constructor ##############
        header_constructor, source_constructor = self.make_header_source(self.name,constructor_content)
        header_destructor = self.make_method("virtual ~"+self.name+"() {}", 2)

        ############## public ##############
        header_init, source_init = self.make_header_source(f'bool Init()', init_content)
        header_exec, source_exec = self.make_header_source(f'void Exec(Option_t *option="")', exec_content)
        header_erun, source_erun = self.make_header_source(f"bool EndOfRun()","return true;")

        header_clear, source_clear = self.make_header_source(f'void Clear(Option_t *option="")', clear_content)
        header_print, source_print = self.make_header_source(f'void Print(Option_t *option="") const', print_content)
        header_copy,  source_copy  = self.make_header_source(f'void Copy(TObject &object) const', copy_content)

        header_dete1, source_dete1 = self.make_header_source('bool BuildGeometry()', "return true;")
        header_dete2, source_dete2 = self.make_header_source('bool BuildDetectorPlane()', dete2_content)

        header_detp0, source_detp0 = self.make_header_source('bool IsInBoundary(Double_t i, Double_t j)', "return true;")
        header_detp1, source_detp1 = self.make_header_source('Int_t FindChannelID(Double_t i, Double_t j)', findch_content)
        header_detp2, source_detp2 = self.make_header_source('TCanvas *GetCanvas(Option_t *option="");', getcvs_content)
        header_detp3, source_detp3 = self.make_header_source('TH2* GetHist(Option_t *option="")', gethist_content)
        header_detp4, source_detp4 = self.make_header_source('bool DrawEvent(Option_t *option="");', "return true;")
        header_detp5, source_detp5 = self.make_header_source('bool SetDataFromBranch()', 'return false;')
        header_detp6, source_detp6 = self.make_header_source('void DrawHist();')
        header_detp7, source_detp7 = self.make_header_source('void DrawFrame(Option_t *option="")')
        header_detp9, source_detp9 = self.make_header_source('void MouseClickEvent(int iPlane);')
        header_detpa, source_detpa = self.make_header_source('void ClickedAtPosition(Double_t x, Double_t y)')

        ############## get set ##############
        if len(self.get_full_list[0])>0: self.get_full_list[0].insert(0,"")
        if len(self.set_full_list[0])>0: self.set_full_list[0].insert(0,"")
        header_getter = tab2 + etab2.join(self.get_full_list[0])
        header_setter = tab2 + etab2.join(self.set_full_list[0])

        ############## public ##############
        header_public_par = tab2 + etab2.join(self.par_def_list[0])

        ############## protected ##############
        header_class_protected = ' '*self.tab_size + "protected:"
        header_protected_par = tab2 + etab2.join(self.par_def_list[1])

        ############## private ##############
        header_class_private = ' '*self.tab_size + "private:"
        header_private_par = ""
        if mode==m_task:
            header_private_par = tab2 + etab2.join(self.data_array_def_list)
        header_private_par = header_private_par + 2*etab2 + etab2.join(self.par_def_list[2])

        ############## other ##############
        header_class_end = "};"
        header_end = "\n#endif"

        header_classdef = self.make_method(f"ClassDef({self.name},1)", 1)#, omit_semicolon=True)
        source_classimp = self.make_method(f"ClassImp({self.name})")#, omit_semicolon=True)

        ############## join header ##############
        if mode==m_task:
            header_list = [
                header_define, header_include_root, header_include_lilak, header_include_other,
                "", header_detail, header_description, header_class,
                header_class_public, header_constructor, header_destructor, header_enum,
                "", header_init, header_exec, header_erun,
                header_getter, header_setter]
        elif mode==m_container:
            header_list = [
                header_define, header_include_root, header_include_lilak, header_include_other,
                "", header_detail, header_description, header_class,
                header_class_public, header_constructor, header_destructor, header_enum,
                "", header_clear, header_print, header_copy,
                header_getter, header_setter]
        elif mode==m_detector:
            header_list = [
                header_define, header_include_root, header_include_lilak, header_include_other,
                "", header_detail, header_description, header_class,
                header_class_public, header_constructor, header_destructor, header_enum,
                "", header_print, header_init,
                header_dete1, header_dete2,
                header_getter, header_setter]
        elif mode==m_detector_plane:
            header_list = [
                header_define, header_include_root, header_include_lilak, header_include_other,
                "", header_detail, header_description, header_class,
                header_class_public, header_constructor, header_destructor, header_enum,
                "", header_print, header_init,
                header_detp0, header_detp1, header_detp2, header_detp3, header_detp4,
                header_detp5, header_detp6, header_detp7, "", header_detp9, header_detpa,
                header_getter, header_setter]

        if len(header_public_par.strip())>0:    header_list.extend(["",header_public_par])
        if len(header_protected_par.strip())>0: header_list.extend(["",header_class_protected, header_protected_par])
        if len(header_private_par.strip())>0:   header_list.extend(["",header_class_private, header_private_par])
        header_list.extend(["",header_classdef,header_class_end,header_end])
        header_all = '\n'.join(header_list)

        ############## join source ##############
        if mode==m_task:
            source_list = [
                source_include,
                "",source_classimp,
                "",source_constructor,
                "",source_init,
                "",source_exec,
                "",source_erun,
                ]
        elif mode==m_container:
            source_list = [
                source_include,
                "",source_classimp,
                "",source_constructor,
                "",source_clear,
                "",source_print,
                "",source_copy
                ]
        elif mode==m_detector:
            source_list = [
                source_include,
                "",source_classimp,
                "",source_constructor,
                "",source_init,
                "",source_print,
                "",source_dete1,
                "",source_dete2
                ]
        elif mode==m_detector_plane:
            source_list = [
                source_include,
                "",source_classimp,
                "",source_constructor,
                "",source_init,
                "",source_print,
                "",source_detp0,
                "",source_detp1,
                "",source_detp2,
                "",source_detp3,
                "",source_detp4,
                "",source_detp5,
                "",source_detp6,
                "",source_detp7,
                "",source_detp9,
                "",source_detpa
                ]
        source_all = '\n'.join(source_list)

        # removing multiple enters
        header_new_list = []
        header_splitlines = header_all.splitlines()
        previous_line_was_empty = False
        for header_line in header_splitlines:
            header_line = header_line.rstrip()
            if len(header_line)==0:
                if previous_line_was_empty:
                    continue
                else:
                    header_new_list.append(header_line)
                    previous_line_was_empty = True
            else:
                header_new_list.append(header_line)
                previous_line_was_empty = False
        header_all = '\n'.join(header_new_list)

        source_new_list = []
        source_splitlines = source_all.splitlines()
        previous_line_was_empty = False
        for source_line in source_splitlines:
            source_line = source_line.rstrip()
            if len(source_line)==0:
                if previous_line_was_empty:
                    continue
                else:
                    source_new_list.append(source_line)
                    previous_line_was_empty = True
            else:
                source_new_list.append(source_line)
                previous_line_was_empty = False
        source_all = '\n'.join(source_new_list)

        ############## Par ##############
        par_all = '\n'.join(self.parfile_lines)

        ############## Print ##############
        name_full = os.path.join(self.path,self.name)
        
        if to_file:
            print(f'Creating {name_full}.h')
            print(f'Creating {name_full}.cpp')
            with open(f'{name_full}.h', 'w') as f1: print(header_all,file=f1)
            with open(f'{name_full}.cpp', 'w') as f1: print(source_all,file=f1)
            if mode==m_task:
                print(f'Creating {name_full}.mac')
                with open(f'{name_full}.mac', 'w') as f1: print(par_all,file=f1)

        if to_screen:
            print(f"{name_full}.h >>>>>")
            print(header_all)
            print(f"\n\n{name_full}.cpp >>>>>")
            print(source_all)
            if mode==m_task:
                print(f"\n\n{name_full}.mac >>>>>")
                print(par_all)

###########################################################################################################################################
    def print_container(self, to_screen=False, to_file=True, print_example_comments=True, includes='', inheritance=''):
        """Print header and source file of lilak container class content to screen or file

        to_screen (bool ; False) -- If True, print container to screen
        to_file (bool ; True) -- If True, print container to file ({path}/{name}.cpp, {path}/{name}.h) 
        print_example_comments (bool ; True) -- Print comments that helps you filling up reset of the class.
        inheritance (string ; 'LKContainer') -- Class inheritance.
        includes (string ; 'LKContainer.h') -- headers to be included separated by space
        """
        self.print_class(mode=1, to_screen=to_screen, to_file=to_file, print_example_comments=print_example_comments, includes=includes, inheritance=inheritance)


###########################################################################################################################################
    def print_task(self, to_screen=False, to_file=True, print_example_comments=True, includes='', inheritance=''):
        """Print header and source file of lilak task class content to screen or file

        to_screen (bool ; False) -- If True, print container to screen
        to_file (bool ; True) -- If True, print container to file ({path}/{name}.cpp, {path}/{name}.h) 
        print_example_comments (bool ; True) -- Print comments that helps you filling up reset of the class.
        """
        self.print_class(mode=0, to_screen=to_screen, to_file=to_file, print_example_comments=print_example_comments, includes=includes, inheritance=inheritance)

###########################################################################################################################################
    def print_detector(self, to_screen=False, to_file=True, print_example_comments=True, includes='', inheritance=''):
        self.print_class(mode=2, to_screen=to_screen, to_file=to_file, print_example_comments=print_example_comments, includes=includes, inheritance=inheritance)

###########################################################################################################################################
    def print_detector_plane(self, to_screen=False, to_file=True, print_example_comments=True, includes='', inheritance=''):
        self.print_class(mode=3, to_screen=to_screen, to_file=to_file, print_example_comments=print_example_comments, includes=includes, inheritance=inheritance)

if __name__ == "__main__":
    help(lilakcc)
