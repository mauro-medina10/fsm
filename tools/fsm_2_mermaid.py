# Author Mauro Medina <mauro93medina@gmail.com>

"""Command-line state machine graph builder library

This script takes a .c file that implemets a finite state machine using the FSM library macros:

    * FSM_STATES_INIT
    * FSM_CREATE_STATE
    * FSM_TRANSITIONS_INIT
    * FSM_TRANSITION_CREATE
    * FSM_TRANSITION_WORK_CREATE
    
Usage: Call script from command line 

    - python fsm_2_mermaid file_name
    
    Where file_name is the .c file that implements the fsm (has to have FSM_CREATE_STATE and FSM_TRANSITION_CREATE somewhere)
    
"""
    
import argparse
import re

class fsm_mermaid:   

    mermaid_head = "```mermaid\n"
    mermaid_format = "stateDiagram-v2\n"
    mermaid_tail = "\n```"
    
    fsm_init_pat = r"FSM_STATES_INIT\((.+?)\)"
    
    fsm_st_none = "FSM_ST_NONE"
    fsm_st_end = "FSM_ST_END"
    fsm_st_first = "FSM_ST_FIRST"
    
    fsm_event_none = "FSM_EV_NONE"
    fsm_event_end = "FSM_END_EV"
    fsm_event_timeout = "FSM_TIMEOUT_EV"
    fsm_event_first = "FSM_EV_FIRST"
    
    fsm_end_dict = {"FSM_END_EV" : "[*]", "FSM_ST_END" : "[*]"}
    
    def __init__(self, fname="example"):
        
        # Set up argument parser
        parser = argparse.ArgumentParser(description="Create a mermaid graph from a fsm file.")
        parser.add_argument("file_name", type=str, help="The name of the C file with the fsm.")
        args = parser.parse_args()

        # Define the file name and content
        self.file_name = args.file_name

        #Count number of fsm in file
        if self.fsm_count() == 0:
            return None
        
        # Open the file in read mode 
        with open(self.file_name+".c", "r") as file:
            self.content = file.read()  # Reads all lines into a list
            
        # Iterate the number for number of fsm
        for self.fsm_new in range(0, self.fsm_total):          
            # Read and get the FSM
            self.fsm_parse()
            
            # Opens or creates the file in write mode and add content
            self.mermaid_file_write()

#------------------------------------------------------------------------------
               
    def fsm_count(self):
        """Counts the number of fsm in file
        """
        # Initialize counter
        self.fsm_total = 0

        # Open and read the file
        with open(self.file_name+".c", "r") as file:
            for line in file:
                # Check if the pattern is found in the line
                if re.search(self.fsm_init_pat, line):
                    self.fsm_total += 1
         
        return self.fsm_total
    
    def fsm_name_get(self):
        # Get the fsm name
        match = re.findall(self.fsm_init_pat, self.content)

        if match[self.fsm_new]:
            self.fsm_name = match[self.fsm_new]
        
    def fsm_states_get(self):
        pattern = rf"FSM_CREATE_STATE\({self.fsm_name},\s*(.+?)\)"
        self.fsm_states = []
        
        # Get the fsm states
        match = re.findall(pattern, self.content)

        if match:
            idx = 0
            elements = match
            for line in match:
                elements[idx] = [item.strip() for item in line.split(',')]
                idx = idx + 1
            self.fsm_states = elements
       
    def fsm_transitions_get(self):
        pattern = rf"FSM_TRANSITION_CREATE\({self.fsm_name},\s*(.+?)\)"
        pattern_w = rf"FSM_TRANSITION_WORK_CREATE\({self.fsm_name},\s*(.+?)\)"
        self.fsm_transitions = []
        self.fsm_transitions_w = []
        
        # Get the fsm transitions
        match = re.findall(pattern, self.content)

        if match:
            idx = 0
            elements = match
            for line in match:
                elements[idx] = [item.strip() for item in line.split(',')]
                idx = idx + 1
                self.fsm_transitions = elements
                
        # Get the fsm transitions with work
        match_w = re.findall(pattern_w, self.content)

        if match_w:
            idx = 0
            elements = match_w
            for line in match_w:
                elements[idx] = [item.strip() for item in line.split(',')]
                idx = idx + 1
                self.fsm_transitions_w = elements
        
    def fsm_states_parse(self):
        self.fsm_st = []
        element_list = []
        idx = 0
        
        for element in self.fsm_states:
            # Parent state
            if element[2] != self.fsm_st_none:    
                element_list.append(element[0])
                element_list.append(element[2])
                for new_el in self.fsm_states:  
                    # Find sub state
                    if new_el[1] == element[0]:
                        element_list.append(new_el[0])
            
                # Add parent and sub states
                self.fsm_st.append(element_list)
                element_list = []
    
    def fsm_parse(self):  
        # Get fsm name      
        self.fsm_name_get()
        
        #Get states
        self.fsm_states_get()
        
        # parse states 
        self.fsm_states_parse()
        
        #Get transitions
        self.fsm_transitions_get()
        
    def mermaid_file_write(self):
        with open(self.fsm_name+".md", "w") as file:
            # Head
            file.write(self.mermaid_head)
            file.write(self.mermaid_format)  
            # States
            for state in self.fsm_st:
                file.write("\tstate ")
                file.write(str(state[0]))
                file.write(" {\n")
                file.write("\t\t[*] --> ")
                file.write(str(state[1])+"\n")

                for sub in state[2:]:
                    # write transitions of sub-states
                    for trans in self.fsm_transitions[:]:
                        if(trans[0] == sub):
                            file.write(f"\t\t {str(sub)} --> {self.fsm_end_dict.get(trans[2], trans[2])} : {self.fsm_end_dict.get(trans[1], trans[1])}\n")
                            # delete transition from self.fsm_transitions
                            self.fsm_transitions.remove(trans)
                    
                file.write("\t}\n\n")
            #Transitions
            if self.fsm_transitions != []:
                for trans in self.fsm_transitions:
                    file.write(f"\t {trans[0]} --> {self.fsm_end_dict.get(trans[2], trans[2])} : {self.fsm_end_dict.get(trans[1], trans[1])}\n")
            #Transitions with work
            if self.fsm_transitions != []:
                for trans in self.fsm_transitions_w:
                    file.write(f"\t {trans[0]} --> {self.fsm_end_dict.get(trans[2], trans[2])} : {self.fsm_end_dict.get(trans[1], trans[1])} / {trans[3]}()\n")
                   
            #tail
            file.write(self.mermaid_tail)
                            
#------------------------------------------------------------------------------

if __name__ == "__main__":
    
    mv = fsm_mermaid()

    raise SystemExit