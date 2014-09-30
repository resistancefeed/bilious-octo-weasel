##Written By Josh Beninson 
## Purpose: Start/Stop/Restart/Checks Status/Stores Commerce Agents on any environment.

#Source infrastructure tools
. /cust/tools/bin/Db2cid.sh

#Set Variables
CURDIR=${PWD}
ECN_NUM=`where_am_i`
USER=`env_client ${ECN_NUM}:app.user`

#Creates files or updates the time stamp if this script has
#already been run on the box
touch "${CURDIR}/.${HOSTNAME}.COMMERCE_AGENT_FILE"
touch "${CURDIR}/.${HOSTNAME}.commerce_agent_error_file"
touch "${CURDIR}/.${HOSTNAME}.stored_commerce_agents"


#Set filename variables
COMMERCE_AGENT_FILE="${CURDIR}/.${HOSTNAME}.commerce_agent_file"
ERROR_FILE="${CURDIR}/.${HOSTNAME}.commerce_agent_error_file"
STORED_AGENTS="${CURDIR}/.${HOSTNAME}.stored_commerce_agents"


##this function extracts all running agents from the list of running processes
running_agents(){
        ps -ef | egrep -o '\-Db2cid\=job\-\S*' | sed 's/\-Db2cid\=job\-//'
}

##this function confirms that the requested action has been completed by checking the list
##of running agents against the list of agents that the action has been requested to be
##performed on
check_process_agent(){
    sleep 10
        if [ $ACTION == start ]; then
                running_agents > $ERROR_FILE
                grep -Fxf $COMMERCE_AGENT_FILE $ERROR_FILE > ${ERROR_FILE}.tmp
                mv ${ERROR_FILE}.tmp $ERROR_FILE

                ## grep -v will invert the matching characteristics of grep
                grep -Fxvf $ERROR_FILE $COMMERCE_AGENT_FILE > ${ERROR_FILE}.tmp
                mv ${ERROR_FILE}.tmp $ERROR_FILE

        else
                running_agents > $ERROR_FILE
                grep -Fxf $COMMERCE_AGENT_FILE $ERROR_FILE > ${ERROR_FILE}.tmp
                mv ${ERROR_FILE}.tmp $ERROR_FILE
        fi

  ##check if error file has been populated
        if [ -s "$ERROR_FILE" ]; then
                echo "The Following Agent(s) Didn't $ACTION....."
                if [ $ACTION == stop ]; then
                    echo "Perhaps A Force-Stop Is Required"
                fi
                echo
                cat "$ERROR_FILE"
                echo
        else
                cd $CURDIR
        fi
}

##this is the function that performs the requested action on the agents
process_agent_array(){

echo
echo -n "Continue? (y/n): "
read CONTINUE

if [[ $CONTINUE =~ ^y$ ]]; then

        echo
        cd /cust/env/${ECN_NUM}/cust/app/commerce_agent-agent/bin

        ##keep first iteration out of to allow for system password authentication process...
        let N=0
                if [[ ${agent_array[${N}]} =~ [0-9]$ ]]; then
                    sudo -u $USER -S ./agentcontrol -j ${agent_array[${N}]} -a $ACTION
                    wait
                else
                    sudo -u $USER ./jobcontrol ${agent_array[${N}]} $ACTION
                    wait
                fi
                ((N++))


        while [ -n "${agent_array[${N}]}" ]; do

                if [[ $ACTION =~ stop$ ]]; then
                        echo "${ACTION}ping ${agent_array[${N}]}"
                else
                        echo "${ACTION}ing ${agent_array[${N}]}"
                fi

                if [[ ${agent_array[${N}]} =~ [0-9]$ ]]; then
                    sudo -u $USER -S ./agentcontrol -j ${agent_array[${N}]} -a $ACTION > /dev/null 2>&1
                    wait
                else
                    sudo -u $USER ./jobcontrol ${agent_array[${N}]} $ACTION > /dev/null 2>&1
                    wait
                fi
                ((N++))


                ##every tenth process will trigger a sleep of 5 seconds to keep load on server down
                # if [ $((${N}%10)) -eq 0 ]; then
                #         sleep 5
                # fi
        done

        check_process_agent

else
echo
echo "Cancelled"
fi
}

X=1
while [ $X -eq 1 ]
do
        unset agent_array

        echo
        echo "_______________COMMERCE AGENT CONTROL SCRIPT________________"
        echo
        echo "Available Options: "
        echo
        echo "0  - Exit"
        echo "1  - List All Currently Running Agents"
        echo "2  - Restart All Running Agents"
        echo "3  - Stop All Running Agents"
        echo "4  - Start All Agents That Were Most Recently Controlled By This Script"
        echo "5  - Stop agent(s)"
        echo "6  - Start agent(s)"
        echo "7  - Force-Stop agent(s) << This should be used as a last resort >>"
        echo "8  - Force-Stop All Running Agents << This should be used as a last resort >>"
        echo "9  - Start all agents in the Stored Agent File"
        echo "10 - Edit/View the Stored Agent File"
        echo "11 - Overwrite the Stored Agent File with All Currently Running Agents"
        echo
        echo -n "Please Enter The Numerical Designation: "
        read selection
        echo

        if [[ ! $selection =~ ^([0-9]|1[0-1])$ ]]; then
                        echo
                        echo "Improper Entry of: $selection"


        elif [ $selection -eq 0 ]; then
                echo
                echo "Exiting................"
                echo
                X=0

        elif [ $selection -eq 1 ]; then
                echo
                echo "The Following Agents Are Running................"
                echo
                running_agents
                echo

        elif [ $selection -eq 2 ]; then
                echo
                echo "Restarting....................................."
                echo
                running_agents | tee $COMMERCE_AGENT_FILE
                echo
                echo "Stopping......................................."

                let N=0
                while IFS=$'\n' read -r agent; do
                        agent_array[${N}]=${agent}
                        ((N++))
                done < "$COMMERCE_AGENT_FILE"

                ACTION="stop"
                process_agent_array
                echo
                echo "STOP PORTION COMPLETE........................."
                echo
                echo
                echo "Starting......................................"
                ACTION="start"
                process_agent_array

                echo
                echo "START PORTION COMPLETE........................."

        elif [ $selection -eq 3 ]; then

                echo
                echo "Stopping......................................"
                running_agents | tee $COMMERCE_AGENT_FILE
                echo
                let N=0
                while IFS=$'\n' read -r agent; do
                        agent_array[${N}]=${agent}
                        ((N++))
                done < "$COMMERCE_AGENT_FILE"

                ACTION="stop"
                process_agent_array

                echo
                echo "STOP PORTION COMPLETE..........................."


        elif [ $selection -eq 4 ]; then
                echo
                echo "The Following Agents Are Being Started..........."

                let N=0
                while IFS=$'\n' read -r agent; do
                        agent_array[${N}]=${agent}
                        ((N++))
                done < "$COMMERCE_AGENT_FILE"

                printf -- '%s\n' "${agent_array[@]}"

                ACTION="start"
                process_agent_array
                echo
                echo "START PORTION COMPLETE........................."


        elif [[ $selection =~ ^[5-7]$ ]]; then
                echo
                echo -n 'Please enter the agent name(s), one per line, and save before exiting'
                sleep 5

                echo
                echo
                vi ${COMMERCE_AGENT_FILE}

                ##remove whitespace from the entries
                sed "s/\s//g" ${COMMERCE_AGENT_FILE} > ${COMMERCE_AGENT_FILE}.tmp
                mv ${COMMERCE_AGENT_FILE}.tmp ${COMMERCE_AGENT_FILE}

                ##remove all empty lines from the entries
                sed '/^$/d' ${COMMERCE_AGENT_FILE} > ${COMMERCE_AGENT_FILE}.tmp
                mv ${COMMERCE_AGENT_FILE}.tmp ${COMMERCE_AGENT_FILE}

                let N=0
                while IFS=$'\n' read -r agent; do
                        agent_array[${N}]=${agent}
                        ((N++))
                done < "$COMMERCE_AGENT_FILE"

        case $selection in
                5)
                echo
                echo "The Following Agents Are Being Stopped............."
                echo
                printf -- '%s\n' "${agent_array[@]}"

                ACTION="stop"
                process_agent_array
                echo
                echo "STOP PORTION COMPLETE........................."
                ;;

                6)
                echo
                echo "The Following Agents Are Being Started.............."
                echo
                printf -- '%s\n' "${agent_array[@]}"

                echo
                ACTION="start"
                process_agent_array
                echo
                echo "START PORTION COMPLETE........................."
                ;;

                7)
                echo
                echo "The Following Agents Are Being Force-Stopped........."
                echo
                printf -- '%s\n' "${agent_array[@]}"

                ACTION="force-stop"
                process_agent_array
                echo
                echo "FORCE-STOP PORTION COMPLETE........................."
                ;;
        esac

        elif [ $selection -eq 8 ]; then
                echo
                echo "Force-Stopping.........................."

                let N=0
                while IFS=$'\n' read -r agent; do
                        agent_array[${N}]=${agent}
                        ((N++))
                done < <(running_agents)

                printf -- '%s\n' "${agent_array[@]}" | tee $COMMERCE_AGENT_FILE

                ACTION="force-stop"
                process_agent_array

                echo
                echo "FORCE-STOP PORTION COMPLETE........................."

        elif [ $selection -eq 9 ]; then
                echo
                                echo "Starting.........................."

                let N=0
                while IFS=$'\n' read -r agent; do
                        agent_array[${N}]=${agent}
                        ((N++))
                done < "$STORED_AGENTS"

                printf -- '%s\n' "${agent_array[@]}"

                ACTION="start"
                process_agent_array

                echo
                echo "START PORTION COMPLETE........................."

        elif [ $selection -eq 10 ]; then
                echo
                echo -n 'Please enter the agent name(s), one per line, and save before exiting'
                sleep 5

                echo
                echo
                vi ${STORED_AGENTS}

                ##removes whitespace from the entries
                sed "s/\s//g" ${STORED_AGENTS} > ${STORED_AGENTS}.tmp
                mv ${STORED_AGENTS}.tmp ${STORED_AGENTS}

                ##removes all empty lines from the entries
                sed '/^$/d' ${STORED_AGENTS} > ${STORED_AGENTS}.tmp
                mv ${STORED_AGENTS}.tmp ${STORED_AGENTS}

                elif [ $selection -eq 11 ]; then

                running_agents
        echo -n "Are You Sure? (y/n): "
        read SURE

                if [[ $SURE =~ ^y$ ]]; then
                echo
                echo "Overwriting....................................."
                echo
                running_agents | tee $STORED_AGENTS
                echo
                echo "OVERWRITE COMPLETE.............................."
                fi

        else
            echo "$selection is not a valid entry. Please try again."

        fi

        unset agent_array


        echo
        echo "__________________END OF ITERATION____________________"


    while [[ $X =~ [^0-1] ]]; do
        echo
        echo "$X Is Not A Valid Response."
        echo -n 'Enter The Numbers "0" To Exit Or "1" To Loop: '
        read X
    done
done
echo
