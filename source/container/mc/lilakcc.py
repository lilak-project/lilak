import os

class lilakcc:
    """
    Write c++ class with source and header
    - methods:
        [x] add(input_lines)
        [x] set_class(line0, acc_spec, method_source)
            [x] set_name()
            [x] set_path()
            [x] set_comment()
            [x] add_inherit_class()
        [x] add_method(line, acc_spec, method_source)
        [-] add_par(line, lname, gname, acc_spec,
                    par_setter, par_getter,
                    par_init, par_clear,
                    par_print, par_source)
        [x] add_par2(par_type, par_name, par_init_val, par_comment, par_name_lc, par_persistency,
                     set_type, set_name, set_comment,
                     get_type, get_name, get_comment, get_is_const, acc_spec,
                     includes, use_fname):
        [x] set_tab_size(tab_size)
    """

    def __init__(self,name=""):
        # class
        self.name = name
        self.path = "./"
        self.comment = ""
        self.tab_size = 4
        self.inherit_list = []

        # for task
        self.data_init_list = []
        self.data_exec_list = []
        self.data_array_def_list = []

        # container 
        self.par_clear_list = []
        self.par_print_list = []
        self.par_copy_list = []

        # general
        self.par_def_list = [[],[],[]]
        self.par_init_list = []
        self.set_full_list = [[],[],[]]
        self.get_full_list = [[],[],[]]
        self.method_header_list = [[],[],[]]
        self.method_source_list = [[],[],[]]

        # include headers
        self.include_root_list = []
        self.include_lilak_list = []
        self.include_other_list = []

        # parameter file
        self.parfile_lines = []
  
    def set_tab_size(self, tab_size):
        """Set tab size"""
        self.tab_size = tab_size

    def set_name(self,name):
        """Set name of the class"""
        self.name = name
  
    def set_file_path(self,file_path):
        """Set path where files are created"""
        self.path = file_path

    def set_comment(self, comment):
        """Description of the class"""
        self.comment = comment 

    def add_inherit_class(self, inherit_acc_class):
        """Description of the class"""
        self.inherit_list.append(inherit_acc_class)

###########################################################################################################################################
    def add(self, input_lines):
        """ Make funciton from line"""

        group = []
        group_list = []
        lines = input_lines.splitlines()
        head_is_found = False

        for line in lines:
            if len(line)==0:
                if len(group)>0:
                    group_list.append(group.copy())
                    head_is_found = False
                    group.clear()

            elif line[:2] == '//' or line[:2] == '/*' or line[:2] == ' *' or line[0] == '*':
                group.append(["comment",line])

            elif line[0] == '+':
                ltype = line[1:line.find(' ')] # line type
                content = line[line.find(' ')+1:] # line type
                content = content.strip()
                if ltype=='class' or ltype=='private' or ltype=='protected' or ltype=='public':
                    if head_is_found == True:
                        if len(group)>0:
                            group_list.append(group.copy())
                            head_is_found = False
                            group.clear()
                    if ltype.find('par/p')==0 or ltype.find('method/p')==0:
                        pass
                    elif ltype in ["private", "protected", "public"]:
                        ib = content.find('(')
                        ie = content.find('=')
                        if   ie<0 and ib<0: ltype = 'par/'+ltype
                        elif ib<0 or ie<ib: ltype = 'par/'+ltype
                        elif ie<0 or ib<ie: ltype = 'method/'+ltype
                    head_is_found = True
                    group.append([ltype,content])

                elif ltype in ["gname", "lname", "setter", "getter", "init", "clear", "source", "print"]:
                    group.append([ltype,content])
            else:
                if len(group)>0:
                    group_list.append(group.copy())
                    head_is_found = False
                    group.clear()

        for group in group_list:
            ltype0, line0 = group[0]

            if ltype0=='class':
                class_path = ""
                class_comment = ""
                for ltype, line in group:
                    if ltype=='path':    class_path = line
                    if ltype=='comment': class_comment = line
                set_class(line0,class_path=class_path,class_comment=class_comment)

            elif ltype0[:5]=='method':
                acc_spec = ltype0[:7]
                for ltype, line in group:
                    method_source = ""
                    if ltype=='source': method_source = line
                add_method(line0, acc_spec=acc_spec, method_source = method_source)

            elif ltype0[:3]=='par':
                acc_spec = ltype0[:4]
                for ltype, line in group:
                    par_lname  = ""
                    par_gname  = ""
                    par_setter = ""
                    par_getter = ""
                    par_init   = ""
                    par_clear  = ""
                    par_print  = ""
                    par_source = ""
                    if ltype=='lname':  par_lname  = line
                    if ltype=='gname':  par_gname  = line
                    if ltype=='setter': par_setter = line
                    if ltype=='getter': par_getter = line
                    if ltype=='init':   par_init   = line
                    if ltype=='clear':  par_clear  = line
                    if ltype=='print':  par_print  = line
                    if ltype=='source': par_source = line
                self.add_par(lines, lname=par_lname, gname=par_gname, acc_spec=acc_spec,
                        par_setter=par_setter, par_getter=par_getter,
                        par_init=par_init, par_clear=par_clear,
                        par_print=par_print, par_source=par_source)

###########################################################################################################################################
    def set_class(self, line, class_path="", class_comment=""):
        """
        1) class class_name
        2) class class_name : public inherit_class
        3) class class_name : public inherit_class1, public inherit_class2, ...
        """
        ba_colon = line.split(':')
        before_colon = ba_colon[0].strip()
        after_colon = ba_colon[1].strip()

        if before_colon.find('class')==0:
            class_name = before_colon[5:].strip()
        else:
            class_name = before_colon.strip()

        set_name(class_name)
        set_path(class_comment)
        set_comment(class_comment)

        inherit_list = after_colon.split(',')
        for inherit_line in inherit_list:
            if len(inherit_line)>5:
                add_inherit_class(inherit_line.spilt())
    
###########################################################################################################################################
    def add_method(self, line, acc_spec="public", method_source=""):
        pass
        if acc_spec=="public":
            self.method_header_list[0].append(method_header)
            self.method_source_list[0].append(method_source)
        if acc_spec=="protected":
            self.method_header_list[1].append(method_header)
            self.method_source_list[1].append(method_source)
        if acc_spec=="private":
            self.method_header_list[2].append(method_header)
            self.method_source_list[2].append(method_source)
        
###########################################################################################################################################
    def add_par(self, lines, lname="public", gname=par_gname, acc_spec=acc_spec,
                par_setter=par_setter, par_getter=par_getter,
                par_init=par_init, par_clear=par_clear,
                par_print=par_print, par_source=par_source)
        """ add parameter
        lines               -- Input contents
        lname = ""          -- Local name to be used inside the block
        gname = ""          -- Global(Field) name used through class. Default is f[lname] if not set.
        acc_spec = "public" -- Access specifier: one of "public", protected", "private"
        par_setter = ""     -- Contents to be add as Getter.
        par_getter = ""     -- Contents to be add as Setter.
        par_init = ""       -- Contents to be add in the Init() method.
        par_clear = ""      -- Contents to be add in the Clear() method.
        par_print = ""      -- Contents to be add in the Print() method.
        par_source = ""     -- Contents to be add in the class constructor.
        """
        if   acc_spec=="public":     ppp_index = 0
        elif acc_spec=="private" :   ppp_index = 2
        elif acc_spec=="protected" : ppp_index = 1
        else:
            print("Error! [add_par], acc_spec should be one of 'public', 'protected', 'priviate'!")
        pass

        
###########################################################################################################################################
    def add_par2(self,
                par_type, par_name, par_init_val, par_comment="", par_name_lc="", par_persistency = True,
                set_type="", set_name="", set_comment="",
                get_type="", get_name="", get_comment="", get_is_const=True, acc_spec="public",
                includes="", use_fname=True):
        """ Add parameter
        par_type     (string)       -- Type of parameter
        par_name     (string)       -- Name of parameter
        par_init_val (string ; "")  -- Initial parameter value. If par_init_val is empty string(""), it will not be initialized
                                       However it is highly recommended that to give initial parameter value to a value that is unlikely to be given.
                                       This will help you in debugging in the analysis step.
        par_name_lc  (string ; "")  -- Local parameter name. By defualt, = par_name or par_name + "_"
        par_comment  (string ; "")  -- Comment to parameter.
        set_type     (string ; "")  -- Setter input type. By defualt, = par_type
        set_name     (string ; "")  -- Setter name. By defualt, = Set + par_name[0].title()+par_name[1:]
        set_comment  (string ; "")  -- Comment to setter
        get_type     (string ; "")  -- Getter type. By defualt, = par_type
        get_name     (string ; "")  -- Getter name. By defualt, = Get + par_name[0].title()+par_name[1:]
        get_comment  (string ; "")  -- Comment to getter
        get_is_const (bool ; True)  -- Set True if getter is const
        acc_spec     (string ; "public") -- Choose access specifier from ("public", "protected", "private")
        includes     (string ; "")  -- Headers to include (separated by space of line-break)
        use_fname    (bool ; True)  -- By default, field (or member) parameter name will be set to
                                       "f" + par_name[0].title()+par_name[1:]. Set use_fname to False if you wish
                                       to use par_name itself.
        return_addr  (bool ; True)  -- Return address of the parameter from Getter
        """

        if   acc_spec=="public":     ppp_index = 0
        elif acc_spec=="private" :   ppp_index = 2
        elif acc_spec=="protected" : ppp_index = 1
        else:
            print("Error! from add_par, you should choose one of public_par, protected_par, private_par to be True!")

        ############ field parameter name ############
        if use_fname:
            if par_name[0]=="f" and par_name[1].isupper:
                par_name = par_name[1:]
            else:
                par_name_field = "f" + par_name[0].title()+par_name[1:]
        else:
            par_name_field = par_name
            if par_name[0]=="f" and par_name[1].isupper:
                par_name = par_name[1:]

        ############ local parameter name ############
        if len(par_name_lc)==0:
            par_name_lc = par_name
            if par_name_lc==par_name_field:
                par_name_lc = par_name_lc + "_"

        ############ parameter task init ############
        if isinstance(par_init_val, str)==False: par_init_val = str(par_init_val)
        init_val_is_clear = False
        if par_init_val.find("{x}")>=0:
            init_val_is_clear = True
            par_init_val = par_init_val.replace("{x}",par_name_field)
        par_file_val = par_init_val

        par_task_init = par_name_field
        par_getpar_comment = ""
        if   par_type=="bool":     par_type_getpar = "Bool"
        elif par_type=="int":      par_type_getpar = "Int"
        elif par_type=="double":   par_type_getpar = "Double"
        elif par_type=="Bool_t":   par_type_getpar = "Bool"
        elif par_type=="Int_t":    par_type_getpar = "Int"
        elif par_type=="Double_t": par_type_getpar = "Double"
        elif par_type=="TString":  par_type_getpar = "String"
        elif par_type=="const char*":  par_type_getpar = "String"
        elif par_type=="Color_t":  par_type_getpar = "Color"
        elif par_type=="Width_t":  par_type_getpar = "Width"
        elif par_type=="Size_t":   par_type_getpar = "Size"
        elif par_type=="TVector3":
            par_type_getpar = "V3"
            par_file_val = par_file_val[par_file_val.find('(')+1:par_file_val.find(')')]
            par_file_val = par_file_val.replace(',','  ')
        else:
            par_getpar_comment = f"//TODO The type {par_type} is not featured with LKParameterContainer. Please modify Below:" 
            par_type_getpar = par_type
        #par_task_init = f'//{par_name_field} = par -> GetPar{par_type_getpar}("{par_name_lc}");'
        par_task_init = f'{par_name_field} = par -> GetPar{par_type_getpar}("{par_name_lc}");'

        ############ parameter definition ############
        init_from_header = True
        if init_val_is_clear:
            init_from_header = False
        elif len(par_init_val)==0:
            init_from_header = False
        elif par_init_val.find('->')>0:
            init_from_header = False
        elif par_init_val.find('.')>0:
            if par_init_val[par_init_val.find('.')+1].isdigit()==False:
                init_from_header = False

        if init_val_is_clear: line_parfile = f'#{par_name_lc} {par_file_val}'
        else:                 line_parfile = f'{par_name_lc} {par_file_val}'
        if init_from_header: par_def = f'{par_type} {par_name_field} = {par_init_val};'
        else:                par_def = f'{par_type} {par_name_field};'
        par_def = self.make_doxygen_comment(par_comment,par_def,is_persistence=par_persistency)

        ############ parameter clear ############
        if init_val_is_clear:
            par_clear = f'{par_init_val};';
        elif len(par_init_val)>0:
            par_clear = f'{par_name_field} = {par_init_val};';
        else:
            par_clear = f'{par_name_field};';

        ############ parameter print ############
        if par_type=="bool" or par_type=="int" or par_type=="double" or par_type=="Bool_t" or par_type=="Int_t" or par_type=="Double_t" or par_type=="TString" or par_type=="const char*":
            par_print = f'lx_info << "{par_name} : " << {par_name_field} << std::endl;'
        else:
            par_print = f'//lx_info << "{par_name} : " << {par_name_field} << std::endl;'

        ############ settter definition ############
        if len(set_type)==0: set_type = par_type
        if len(set_name)==0: set_name = "Set" + par_name[0].title()+par_name[1:]
        set_full = self.make_function("void", set_name, f"{set_type} {par_name_lc}", f"{par_name_field} = {par_name_lc};", 0, set_comment, in_line=True)
  
        ############ gettter definition ############
        if len(get_type)==0: get_type = par_type
        if len(get_name)==0: get_name = "Get" + par_name[0].title()+par_name[1:]
        #get_full = self.make_function(get_type, get_name, "", f"return {par_name_field};", 0, get_comment, func_is_const=get_is_const, in_line=True)
        if return_addr:
            get_full = self.make_fline(f"{get_type} *{get_name}() " +"{"+ f"return &{par_name_field};"+"}", in_line=True)
        else:
            get_full = self.make_fline(f"{get_type} {get_name}() const " +"{"+ f"return {par_name_field};"+"}", in_line=True)
  
        ############ parameter copy ############
        par_copy = f"objCopy.{set_name}({par_name_field})"

        ############ gettter definition ############
        if len(par_getpar_comment)!=0:
            self.par_init_list.append(par_getpar_comment);
        self.par_init_list.append(par_task_init);
        self.par_clear_list.append(par_clear);
        self.par_print_list.append(par_print);
        self.par_copy_list.append(par_copy);
        self.par_def_list[ppp_index].append(par_def);
        self.set_full_list[0].append(set_full);
        self.get_full_list[0].append(get_full);

        ############ include includes ############
        self.include_headers(includes)

        ############ parameter container file ############
        self.parfile_lines.append(line_parfile)

    def make_fline(self, func_full, func_comment="", tab_no=-1, is_source=False, is_header=False, omit_semicolon=False, in_line=False):
        """Make funciton from line"""
        func_type, func_name, func_parameters, func_content, tab_no2, func_comment2, func_is_const = self.break_line(func_full)

        if   len(func_comment)==0 and len(func_comment2)!=0: func_comment = func_comment2
        elif len(func_comment)!=0 and len(func_comment2)==0: pass
        elif len(func_comment)!=0 and len(func_comment2)!=0: func_comment = func_comment + '\n' + func_comment2

        if   tab_no<-1 and tab_no2>=0: tab_no = tab_no2
        elif tab_no>=0: tab_no = tab_no
        else: tab_no = 0

        return self.make_function(func_type, func_name, func_parameters, func_content, tab_no, func_comment, func_is_const, is_header, is_source, omit_semicolon, in_line)
###########################################################################################################################################

    def break_line(self,func_full):
        func_noc = func_full
        func_prec_list = []

        ic1 = func_noc.find("/*")
        while ic1>=0:
            ic2 = func_noc.find("*/")
            func_foundblock = func_noc[ic1:ic2+2]
            func_noc = func_noc[ic2+3:]
            ic1 = func_noc.find("/*")
            if func_foundblock.isspace()==False:
                for line in func_foundblock.splitlines():
                    line = line.strip()
                    if line!="/**" and line!="*/" and line!="/*":
                        if   line.find("/** ")==0:  line = line[4:]
                        elif line.find("/**")==0:   line = line[3:]
                        if   line.find("/* ")==0:   line = line[3:]
                        elif line.find("/*")==0:    line = line[2:]
                        if   line.find("*/ ")==0:   line = line[3:]
                        elif line.find("*/")==0:    line = line[2:]
                        if   line.find("* ")==0:    line = line[2:]
                        elif line.find("*")==0:     line = line[1:]
                        func_prec_list.append(line)

        ib1 = func_noc.find("(")
        ic1 = func_noc.find("//")
        while ic1>=0 and ic1<ib1:
            ic2 = func_noc.find("\n")
            if ic1==func_noc.find("///"):   func_linec = func_noc[ic1+3:ic2]
            else:                           func_linec = func_noc[ic1+2:ic2]
            func_noc = func_noc[ic2+1:]
            func_prec_list.append(func_linec)
            ib1 = func_noc.find("(")
            ic1 = func_noc.find("//")

        func_prec = '\n'.join(func_prec_list) if len(func_prec_list)!=0 else ""

        ib1 = func_noc.find("(")
        if ib1<0: ib1 = len(func_noc)
        ispace = func_noc[:ib1].rfind(" ")
        while ispace==ib1-1:
            func_noc = func_noc[:ispace]+func_noc[ib1:]
            ib1 = func_noc.find("(")
            if ib1<0: ib1 = len(func_noc)
            ispace = func_noc[:ib1].rfind(" ")

        if ispace<0: iname = 0
        else: iname = ispace + 1

        ib2 = func_noc.rfind(")")
        if ib2<0: ib2 = len(func_noc)

        ib3 = func_noc.find("{")
        ib4 = func_noc.find("}")

        func_pstc = ""
        if ib4<0: func_aft4 = func_noc[ib2+1:]
        else:     func_aft4 = func_noc[ib4+1:]
        ic3 = func_aft4.find("//")
        if ic3>=0:
            ic4 = func_aft4.find("///<")
            if ic4>=0: func_pstc = func_aft4[ic4+4:]
            else:      func_pstc = func_aft4[ic3+2:]
        func_pstc = func_pstc.strip()

        func_name = func_noc[iname:ib1]
        if iname>0: func_type = func_noc[:iname-1]
        else: func_type = ""

        func_parameters = func_noc[ib1+1:ib2]
        if ib3>0:
            func_content = func_noc[ib3+1:ib4]
            func_is_const = True if func_noc[ib2+1:ib3].find("const")>=0 else False
            if func_content[0]=="\n": func_content = func_content[1:]
            if func_content[len(func_content)-1]=="\n": func_content = func_content[:len(func_content)-1]
        else:
            func_content = ""
            func_is_const = True if func_noc[ib2+1:].find("const")>=0 else False

        func_prec_list.append(func_pstc)
        func_comment = '\n'.join(func_prec_list)

        tab_no = 0 #todo

        return func_type, func_name, func_parameters, func_content, tab_no, func_comment, func_is_const

    def make_function(self, func_type, func_name, func_parameters, func_content="", tab_no=0, func_comment="", func_is_const=False, is_header=False, is_source=False, omit_semicolon=False, in_line=False):
        """Make c++ function with given parameters

        func_type       (string) -- data type of function
        func_name       (string) -- name of the function
        func_parameters (string) -- input parameters of function
        func_content    (string; "") -- content of the function
        tab_no          (string ; 0) -- number of tabs (indents) of the function
        func_comment    (string ; "") -- comment of the function
        func_is_const   (bool ; False) -- Put const after the function definition
        is_header       (bool ; False) -- Make header file
        is_source       (bool ; False) -- Make source file
        in_line         (bool ; False) -- Make function in-line
        omit_semicolon  (bool ; False) -- Omit ";"
        """
        if is_source and func_name.find("::")<0: func_full = self.name + "::" + func_name + "(" + func_parameters + ")"
        else:                                    func_full = func_name + "(" + func_parameters + ")"

        if len(func_type)>0: func_full = ' '*(self.tab_size*tab_no) + func_type + " " + func_full
        else:                func_full = ' '*(self.tab_size*tab_no) + func_full

        if (func_is_const):
            func_full = func_full + " const"

        lines = func_content.split('\n')

        just_define = False
        mult_line = False
        if in_line==True: pass
        elif is_header:             just_define = True
        elif is_source:             mult_line = True
        elif len(func_content)==0:  just_define = True
        else:                       mult_line = True

        if just_define:
            if omit_semicolon==False:
                func_full = func_full + ";"
        elif mult_line:
            if func_content=="":
                func_full = func_full + '\n' + ' '*(self.tab_size*tab_no) + "{"
                func_full = func_full + '\n' + ' '*(self.tab_size*tab_no) + "}"
            else:
                for i in range(len(lines)):
                    lines[i] = ' '*(self.tab_size*(tab_no+1)) + lines[i]
                func_full = func_full + '\n' + ' '*(self.tab_size*tab_no) + "{"
                func_full = func_full + '\n' + '\n'.join(lines)
                func_full = func_full + '\n' + ' '*(self.tab_size*tab_no) + "}"
        else:
            if func_content.isspace():  func_full =  func_full + " {}"
            else:                       func_full =  func_full + " { " + func_content + " }"
        return self.make_doxygen_comment(func_comment, func_full)

    def make_doxygen_comment(self, comment, add_to="", always_mult_line=False, not_for_doxygen=False, is_persistence=True):
        """Make doxygen comment

        add_to (string) -- If add_to parameter is set True, comment will be put after (before)
                           for the case comment is one-line (multi-line) and return it.
                           Return comment, is_mult_line where is_mult_line is True when comment is multi-line if add_to is set False. 
        always_mult_line (bool) -- The comments are assumed that it is multi-line and use /** [...] */
        not_for_doxygen (bool) -- If True: /** -> /*, ///< -> //
        """
        if is_persistence and len(comment)==0:
            return add_to
        else:
            if always_mult_line or "\n" in comment or "\r\n" in comment:
                lines = comment.split('\n')
                for i in range(len(lines)):
                    lines[i] = ' * ' + lines[i]
                else:
                    if (not_for_doxygen): lines.insert(0,"/*")
                    else:                 lines.insert(0,"/**")
                    lines.append(" */")
                comment = '\n'.join(lines)
                return comment + add_to
            else:
                if is_persistence:
                    comment = " ///< " + comment;
                else:
                    comment = " ///<! " + comment;
                return add_to + comment

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

            if header[0]=="T":      self.include_root_list.append(header_full)
            elif header[:2]=="LK":  self.include_lilak_list.append(header_full)
            else:                   self.include_other_list.append(header_full)

    def add_input_data_array(self,
                             data_class, data_array_name, data_array_branch_name,
                             data_array_comment="", data_array_name_lc="", data_name="data",
                             includes="", use_fname=True):
        ############ field name ############
        if use_fname:
            if data_array_name[0]=="f" and data_array_name[1].isupper:
                par_name = par_name[1:]
            else:
                data_array_name_field = "f" + data_array_name[0].title()+data_array_name[1:]
        else:
            data_array_name_field = data_array_name
            if data_array_name[0]=="f" and data_array_name[1].isupper:
                par_name = par_name[1:]

        ############ local name ############
        if len(data_array_name_lc)==0:
            data_array_name_lc = data_array_name
            if data_array_name_lc==data_array_name_field:
                data_array_name_lc = data_array_name_lc + "_"

        self.data_init_list.append(f'fTrackArray = run -> GetBranchA("{data_array_branch_name}");')

        num_data = "num" + data_array_branch_name[0].title()+data_array_branch_name[1:]
        i_data = "i" + data_array_branch_name[0].title()+data_array_branch_name[1:]
        tab1 = ' '*(self.tab_size*1)
        self.data_exec_list.append(f"""
//Call {data_name} from {data_array_name_field} and get data value
int {num_data} = {data_array_name_field} -> GetEntriesFast();
for (int {i_data} = 0; {i_data} < {num_data}; ++{i_data})""" + "\n{" + f"""
{tab1}auto *{data_name} = ({data_class} *) {data_array_name_field} -> At({i_data});
{tab1}//auto value = {data_name} -> GetDataValue(); ...
"""+"}")
        self.include_headers(includes)


    def add_output_data_array(self,
                              data_class, data_array_name, data_array_branch_name,
                              data_array_init_size=0, data_array_comment="", data_array_name_lc="", data_name="data",
                              data_persistency=True, includes="", use_fname=True):
        ############ field name ############
        if use_fname:
            if data_array_name[0]=="f" and data_array_name[1].isupper:
                par_name = par_name[1:]
            else:
                data_array_name_field = "f" + data_array_name[0].title()+data_array_name[1:]
        else:
            data_array_name_field = data_array_name
            if data_array_name[0]=="f" and data_array_name[1].isupper:
                par_name = par_name[1:]

        ############ persistency ############
        data_array_name_persis = data_array_name_field + "Persistency"
        data_array_name_persis_lc = data_array_name_lc + "Persistency"
        self.add_private_par("bool", data_array_name_persis_lc, "true" if data_persistency else "false")

        ############ branch name ############
        #if len(data_array_branch_name)==0: data_array_branch_name = data_array_name;

        ############ local name ############
        if len(data_array_name_lc)==0:
            data_array_name_lc = data_array_name
            if data_array_name_lc==data_array_name_field:
                data_array_name_lc = data_array_name_lc + "_"

        self.data_array_def_list.append(f"TClonesArray *{data_array_name_field} = nullptr;")

        if data_array_init_size>0: self.data_init_list.append(f'{data_array_name_field} = new TClonesArray("{data_class}");')
        else:                      self.data_init_list.append(f'{data_array_name_field} = new TClonesArray("{data_class}",{data_array_init_size});')
        self.data_init_list.append(f'run -> RegisterBranch("{data_array_branch_name}", {data_array_name_field}, {data_array_name_persis});')

        num_data = "num" + data_array_branch_name[0].title()+data_array_branch_name[1:]
        i_data = "i" + data_array_branch_name[0].title()+data_array_branch_name[1:]
        tab1 = ' '*(self.tab_size*1)
        self.data_exec_list.append(f"""
//Construct (new) {data_name} from {data_array_name_field} and set data value
int {num_data} = {data_array_name_field} -> GetEntriesFast();
for (int {i_data} = 0; {i_data} < {num_data}; ++{i_data})""" + "\n{" + f"""
{tab1}auto *{data_name} = ({data_class} *) {data_array_name_field} -> ConstructedAt({i_data});
{tab1}//{data_name} -> SetData(value); ...
"""+"}")
        self.include_headers(includes)

    def init_print(self):
        if os.path.exists(self.path)==False:
            os.mkdir(self.path,exist_ok=True) 

    def print_container(self,to_screen=False,to_file=True,print_example_comments=True,
                        inheritance='public LKContainer',
                        includes='LKContainer.hpp'):
        """Print header and source file of lilak container class content to screen or file

        to_screen (bool ; False) -- If True, print container to screen
        to_file (bool ; True) -- If True, print container to file ({path}/{name}.cpp, {path}/{name}.hpp) 
        print_example_comments (bool ; True) -- Print comments that helps you filling up reset of the class.
        inheritance (string ; 'LKContainer') -- Class inheritance.
        includes (string ; 'LKContainer.hpp') -- headers to be included separated by space
        """
        self.init_print()

        self.include_headers('TClonesArray.h')
        self.include_headers('LKLogger.hpp')
        self.include_headers(includes)
        self.include_headers('<iostream>')

        inherit_class_list = inheritance.split(',')
        inherit_class = inherit_class_list[0]
        inherit_class = inherit_class.replace('public',' ')
        inherit_class = inherit_class.replace('private',' ')
        inherit_class = inherit_class.replace('private',' ')
        inherit_class = inherit_class.strip()

        tab1 = ' '*(self.tab_size*1)
        tab2 = ' '*(self.tab_size*2)
        etab1 = '\n'+tab1
        etab2 = '\n'+tab2

        name_upper = self.name.upper()
        header_define = f"""#ifndef {name_upper}_HH
#define {name_upper}_HH
"""
        header_container="""Remove this comment block after reading it through
Or use print_example_comments=False option to omit printing

# Example LILAK container class

## Essential
 - Write constructor containing Clear() method.
 - Write Clear() method.

More about Clear() method:
It is recommended that LKTasks create data container array using TClonesArray class.
For the detail of TClonesArray, see https://root.cern/doc/master/classTClonesArray.html
or https://opentutorials.org/module/2860/19477 (for the simple version in Korean)
In LKTask, Clear("C") is called to TClonesArray at the start of the Exec() of LKTask classes.
This means that all containers 

Version number (2nd par. in ClassDef of source file) should be changed if the class has been modified.
This notifiy users that the container has been update in the new LILAK (or side project version).

## Recommended
 - Documentaion like this!
 - Write Print() to see what is inside the container;

## If you have time
 - Write Copy() for copying object
"""
        if print_example_comments:
            header_container = self.make_doxygen_comment(header_container,not_for_doxygen=True)
            header_container = header_container + "\n"
        else:
            header_container = ""
        header_include_lilak = '\n'.join(sorted(set(self.include_lilak_list)))
        header_include_root = '\n'.join(sorted(set(self.include_root_list)))
        header_include_other = '\n'.join(sorted(set(self.include_other_list)))
        header_description = self.make_doxygen_comment(self.comment)
        header_class = f"class {self.name} : {inheritance}" + "\n{"

        source_include = f'#include "{self.name}.hpp"'

        ############## public ##############
        header_class_public = ' '*self.tab_size + "public:"
        if print_example_comments:
            constructor_content = "// It is essential to place Clear() method inside the constructor for TClonesArray feature.\nClear();"
        else:
            constructor_content = "Clear();"
        header_constructor = self.make_fline(self.name, tab_no=2, is_header=True)
        header_destructor = self.make_fline("virtual ~"+self.name, tab_no=2, is_header=True)
        header_clear = self.make_fline("virtual void Clear(Option_t *option)", tab_no=2, is_header=True)
        header_print = self.make_fline("virtual void Print(Option_t *option) const;", tab_no=2, is_header=True)
        header_copy = self.make_fline("virtual void Copy (TObject &object) const;",  tab_no=2, is_header=True)

        header_getter = tab2 + etab2.join(self.get_full_list[0])
        header_setter = tab2 + etab2.join(self.set_full_list[0])
        header_public_par = tab2 + etab2.join(self.par_def_list[0])

        self.par_clear_list.insert(0,f"{inherit_class}::Clear(option);")
        clear_content = '\n'.join(self.par_clear_list)

        self.par_print_list.insert(0,"//TODO You will probability need to modify here")
        self.par_print_list.insert(1,f"{inherit_class}::Print();")
        self.par_print_list.insert(2,f'lx_info << "{self.name} container" << std::endl;')
        print_content = '\n'.join(self.par_print_list)

        self.par_copy_list.insert(0,"//TODO You should copy data from this container to objCopy")
        self.par_copy_list.insert(1,f"{inherit_class}::Copy(object);")
        self.par_copy_list.insert(2,f"auto objCopy = ({self.name} &) object;")
        copy_content = '\n'.join(self.par_copy_list)

        source_constructor = self.make_function("", self.name, "", constructor_content, 0, is_source=True)
        source_clear = self.make_function("void", "Clear", "Option_t *option", clear_content, 0, is_source=True)
        source_print = self.make_function("void", "Print", "Option_t *option", print_content, 0, "", True, is_source=True)
        source_copy = self.make_function ("void", "Copy",  "TObject &object",  copy_content, 0, "", True, is_source=True)

        ############## protected ##############
        header_class_protected = ' '*self.tab_size + "protected:"
        header_protected_par = tab2 + etab2.join(self.par_def_list[1])

        ############## private ##############
        header_class_private = ' '*self.tab_size + "private:"
        header_private_par = tab2 + etab2.join(self.par_def_list[2])

        ############## other ##############
        header_class_end = "};"
        header_end = "\n#endif"

        header_classdef = self.make_fline(f"ClassDef({self.name},0)", tab_no=1, omit_semicolon=True)
        source_classimp = self.make_fline(f"ClassImp({self.name})", omit_semicolon=True)

        ############## join header ##############
        header_list = [
            header_define, header_container,
            header_include_root, header_include_lilak, header_include_other,
            "",header_description, header_class,
            header_class_public, header_constructor, header_destructor,
            "", header_clear, header_print, header_copy,
            "", header_getter,
            "", header_setter]
        if len(header_public_par.strip())>0:    header_list.extend(["", header_public_par])
        if len(header_protected_par.strip())>0: header_list.extend(["",header_class_protected, header_protected_par])
        if len(header_private_par.strip())>0:   header_list.extend(["",header_class_private, header_private_par])
        header_list.extend(["",header_classdef,header_class_end,header_end])
        header_all = '\n'.join(header_list)

        ############## join source ##############
        source_list = [
            source_include,
            "",source_classimp,
            "",source_constructor,
            "",source_clear,
            "",source_print,
            "",source_copy
            ]
        source_all = '\n'.join(source_list)

        ############## Par ##############
        par_all = '\n'.join(self.parfile_lines)

        ############## Print ##############
        name_full = os.path.join(self.path,self.name)

        if to_file:
            print(name_full)
            with open(f'{name_full}.hpp', 'w') as f1: print(header_all,file=f1)
            with open(f'{name_full}.cpp', 'w') as f1: print(source_all,file=f1)
            #with open(f'{name_full}.par', 'w') as f1: print(par_all,file=f1)

        if to_screen:
            print(f"{name_full}.hpp >>>>>")
            print(header_all)
            print(f"\n\n{name_full}.cpp >>>>>")
            print(source_all)


    def print_task(self,to_screen=False,to_file=True,print_example_comments=True):
        """Print header and source file of lilak task class content to screen or file

        to_screen (bool ; False) -- If True, print container to screen
        to_file (bool ; True) -- If True, print container to file ({path}/{name}.cpp, {path}/{name}.hpp) 
        print_example_comments (bool ; True) -- Print comments that helps you filling up reset of the class.
        """
        self.init_print()

        self.include_headers('TClonesArray.h')
        self.include_headers('LKLogger.hpp')
        self.include_headers('LKTask.hpp')
        self.include_headers('LKParameterContainer.h')
        self.include_headers('LKRun.h')
        self.include_headers('<iostream>')

        tab1 = ' '*(self.tab_size*1)
        tab2 = ' '*(self.tab_size*2)
        etab1 = '\n'+tab1
        etab2 = '\n'+tab2

        name_upper = self.name.upper()
        header_define = f"""#ifndef {name_upper}_HH
#define {name_upper}_HH
"""
        header_LKTask="""Remove this comment block after reading it through
Or use print_example_comments=False option to omit printing

# Example LILAK task class

## Essential
 - Write Init() method.
 - Write Exec() method using Clear() method to all clones arrays.
"""
        if print_example_comments:
            header_LKTask = self.make_doxygen_comment(header_LKTask,not_for_doxygen=True) + "\n"
        else:
            header_LKTask = ""
        header_include_lilak = '\n'.join(sorted(set(self.include_lilak_list)))
        header_include_root = '\n'.join(sorted(set(self.include_root_list)))
        header_include_other = '\n'.join(sorted(set(self.include_other_list)))
        header_description = self.make_doxygen_comment(self.comment)
        header_class = f"class {self.name} : public LKTask" + "\n{"

        source_include = f'#include "{self.name}.hpp"'

        ############## public ##############
        header_class_public = ' '*self.tab_size + "public:"
        constructor_content = ""
        header_constructor = self.make_fline(self.name, tab_no=2, is_header=True)
        header_destructor = self.make_fline("virtual ~"+self.name, tab_no=2, is_header=True)
        header_init = self.make_fline("bool Init()", tab_no=2, is_header=True)
        header_exec = self.make_fline("void Exec(Option_t *option)", tab_no=2, is_header=True)

        header_getter = tab2 + etab2.join(self.get_full_list[0])
        header_setter = tab2 + etab2.join(self.set_full_list[0])
        header_public_par = tab2 + etab2.join(self.par_def_list[0])

        self.par_init_list.insert(0,"//TODO Put intialization todos here which are not iterative job though event")
        self.par_init_list.insert(1,f'lk_info << "Initializing {self.name}" << std::endl;')
        self.par_init_list.insert(2,"")
        self.par_init_list.insert(3,"auto run = LKRun::GetRun();")
        self.par_init_list.insert(4,"auto par = run -> GetPar();")
        self.par_init_list.insert(5,"")
        init_content = '\n'.join(self.par_init_list)

        self.data_exec_list.insert(0,"//TODO Put the main task job here. Exec() method will be executed for every event.")
        self.data_exec_list.insert(1,"//All the Clear() methods of output data array are usually called at the start of the Exec()")
        self.data_exec_list.append("")
        self.data_exec_list.append(f'lk_info << "{self.name} container" << std::endl;')
        exec_content = '\n'.join(self.data_exec_list)

        source_constructor = self.make_function("", self.name, "", constructor_content, 0, is_source=True)
        source_init = self.make_function("bool", "Init", "", init_content, 0, is_source=True)
        source_exec = self.make_function("void", "Exec", "Option_t *option", exec_content, 0, "", True, is_source=True)

        ############## protected ##############
        header_class_protected = ' '*self.tab_size + "protected:"
        header_protected_par = tab2 + etab2.join(self.par_def_list[1])

        ############## private ##############
        header_class_private = ' '*self.tab_size + "private:"
        header_private_par = tab2 + etab2.join(self.data_array_def_list)
        header_private_par = tab2 + etab2.join(self.par_def_list[2])

        ############## other ##############
        header_class_end = "};"
        header_end = "\n#endif"

        header_classdef = self.make_fline(f"ClassDef({self.name},0)", tab_no=1, omit_semicolon=True)
        source_classimp = self.make_fline(f"ClassImp({self.name})", omit_semicolon=True)

        ############## join header ##############
        header_list = [
            header_define, header_LKTask,
            header_include_root, header_include_lilak, header_include_other,
            "",header_description, header_class,
            header_class_public, header_constructor, header_destructor,
            "", header_init, header_exec,
            "", header_getter,
            "", header_setter]
        if len(header_public_par.strip())>0:     header_list.extend(["", header_public_par])
        if len(header_protected_par.strip())>0:  header_list.extend(["",header_class_protected, header_protected_par])
        if len(header_private_par.strip())>0:    header_list.extend(["",header_class_private, header_private_par])
        header_list.extend(["",header_classdef,header_class_end,header_end])
        header_all = '\n'.join(header_list)

        ############## join source ##############
        source_list = [
            source_include,
            "",source_classimp,
            "",source_constructor,
            "",source_init,
            "",source_exec,
            ]
        source_all = '\n'.join(source_list)

        ############## Par ##############
        par_all = '\n'.join(self.parfile_lines)

        ############## Print ##############
        name_full = os.path.join(self.path,self.name)

        
        if to_file:
            print(name_full)
            with open(f'{name_full}.hpp', 'w') as f1: print(header_all,file=f1)
            with open(f'{name_full}.cpp', 'w') as f1: print(source_all,file=f1)
            with open(f'{name_full}.par', 'w') as f1: print(par_all,file=f1)

        if to_screen:
            print(f"{name_full}.hpp >>>>>")
            print(header_all)
            print(f"\n\n{name_full}.cpp >>>>>")
            print(source_all)


if __name__ == "__main__":
    help(lilakcc)
