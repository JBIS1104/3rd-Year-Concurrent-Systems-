// 10936972_Park_Task4_v4.cpp
// Student ID: 10936972
// Name: Park
// Task 4: UDP Monitor Application
// This program listens for UDP messages from the race simulation and displays the status of the race.

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <iomanip> 
#include <sstream> 
#include "asio.hpp"

using asio::ip::udp;
using namespace std;

// Structure to hold all the information for a single team
struct Team {
    string name;            // Name of the country
    string splits[4];     // Array to store time for each of the 4 legs
    float fumbletime;    // Total time added due to fumbles
    string status;          // Current status message (e.g., "Running", "Finished")
    bool Winner;            // Flag to check if this team has won
};

// Global vector to store the state of our 4 teams
// Initialized with default values
vector<Team> v_teams = {
    {"Jamaica",       {"-", "-", "-", "-"}, 0.0f, "Waiting", false},
    {"United States", {"-", "-", "-", "-"}, 0.0f, "Waiting", false},
    {"Great Britain", {"-", "-", "-", "-"}, 0.0f, "Waiting", false},
    {"Switzerland",   {"-", "-", "-", "-"}, 0.0f, "Waiting", false}
};

// Vector to keep a history of important events like fumbles and DQs
vector<string> events; 

void clearScreen() {
    system("cls");
}

// This function figures out which team a message belongs to.
// It checks for the country name first, then checks for individual athlete names.
int getTeam(string msg) {
    // 1. Check for explicit Team Names in the message
    if (msg.find("Jamaica")       != string::npos) return 0;
    if (msg.find("United States") != string::npos) return 1;
    if (msg.find("Great Britain") != string::npos) return 2;
    if (msg.find("Switzerland")   != string::npos) return 3;

    // 2. Check for Athlete Names (We need this because fumble messages might only have the player name)
        // Jamaica Team Members
    if (msg.find("Williams")        != string::npos || msg.find("Thompson-Herah")   != string::npos || 
        msg.find("Fraser-Pryce")    != string::npos || msg.find("Jackson")          != string::npos) return 0;
        // United States Team Members
    if (msg.find("Oliver")          != string::npos || msg.find("Daniels")          != string::npos || 
        msg.find("Prandini")        != string::npos || msg.find("Thomas")           != string::npos) return 1;
        // Great Britain Team Members
    if (msg.find("Philip")          != string::npos || msg.find("Lansiquot")        != string::npos || 
        msg.find("Asher-Smith")     != string::npos || msg.find("Neita")            != string::npos) return 2;
        
    // Switzerland Team Members
    if (msg.find("Del-Ponte")       != string::npos || msg.find("Kambundji")        != string::npos || 
        msg.find("Kora")            != string::npos || msg.find("Dietsche")         != string::npos) return 3;

    return -1; // Team not found
}



// Helper function to calculate the total time for a team.
// We have to do this locally because the sender doesn't always send the total.
string getTotaltime(int idx) {
    float total = 0.0f;
    try {
        // Loop through all 4 legs
        for(int i=0; i<4; i++) {
            string t = v_teams[idx].splits[i];  
            
            // If any leg is missing or DQ, we can't calculate total yet
            if (t == "-" || t == "DQ") return "N/A"; 
            
            // Remove the 's' from the string to convert to float
            size_t sPos = t.find('s');
            if (sPos != string::npos) t = t.substr(0, sPos);
            
            total += stof(t); // Add leg time
        }
        

        // Add penalties from fumbles
        total += v_teams[idx].fumbletime;
        
        // Format to 2 decimal places
        stringstream stream;
        stream << fixed << setprecision(2) << total;
        return stream.str();
    } catch (...) {
        return "Error";
    }
}

// Main logic to parse the incoming UDP message and update the team state
void processMessage(string msg) {
    int idx = getTeam(msg);
    if (idx == -1) return; // Ignore messages that don't match a team

    // ---------------------------------------------------------
    // CASE 1: LEG UPDATES
    // Example Msg: "Leg 1: Oliver ran in 11.234 seconds."
    // ---------------------------------------------------------
    if (msg.find("Leg") != string::npos && msg.find("ran in") != string::npos) {
        
        // Extract Leg Number
        size_t legPos = msg.find("Leg ");
        int legNum = stoi(msg.substr(legPos + 4, 1)); 

        // Extract Time
        size_t runPos = msg.find("ran in ");
        size_t secPos = msg.find(" seconds");
        if (runPos != string::npos && secPos != string::npos) {
            string timeStr = msg.substr(runPos + 7, secPos - (runPos + 7));
            // Store just the first 5 chars (e.g. 11.23) + "s"
            v_teams[idx].splits[legNum - 1] = timeStr.substr(0, 5) + "s";
        }

        // Update Status
        if (legNum == 4) {
            // Race finished for this team
            string total = getTotaltime(idx);
            string finishStr = "Finished (" + total + "s)";
            
            if (v_teams[idx].Winner) {
                v_teams[idx].status = "WINNER! " + finishStr;
            } else {
                v_teams[idx].status = finishStr;
            }
        } else {
            // Still running
            v_teams[idx].status = "Running (Leg " + to_string(legNum+1) + ")";
        }
    }

    // ---------------------------------------------------------
    // CASE 2: FUMBLES
    // Example Msg: "Oh No! Baton Fumbled by [Name] ! The team [Team] is delayed by: [Delay]s"
    // ---------------------------------------------------------
    if (msg.find("FUMBLED") != string::npos || msg.find("Fumbled") != string::npos) {
        size_t startName = msg.find("Baton Fumbled by "); 
        size_t endName = msg.find(" ! The team");
        size_t startDelay = msg.find("delayed by: "); 
        
        if (startName != string::npos && endName != string::npos && startDelay != string::npos) {
            // Parse player name and delay
            string player = msg.substr(startName + 17, endName - (startName + 17)); 
            string delayStr = msg.substr(startDelay + 12); 
            
            // Add to penalty
            try {
                v_teams[idx].fumbletime += stof(delayStr);
            } catch(...) {}

            // Format delay string
            size_t sPos = delayStr.find("s");
            if(sPos != string::npos) delayStr = delayStr.substr(0, sPos+1);
            else delayStr = delayStr.substr(0, 4) + "s";

            // Log event and update status
            events.push_back("[FUMBLE] " + v_teams[idx].name + ": " + player + " (+" + delayStr + ")");
            v_teams[idx].status = "FUMBLED! (" + delayStr + ")"; 
        }
    }

    // ---------------------------------------------------------
    // CASE 3: DROPPED BATON (DISQUALIFICATION)
    // Example Msg: "No Way! The Baton Dropped From [Name] ! Team..."
    // ---------------------------------------------------------
    if (msg.find("DROPPED") != string::npos || msg.find("Dropped") != string::npos) {
        size_t startName = msg.find("Dropped From ");
        size_t endName = msg.find(" ! Team");
        
        string player = "Unknown";
        if (startName != string::npos && endName != string::npos) {
             player = msg.substr(startName + 13, endName - (startName + 13)); 
        }

        // Mark as DQ
        v_teams[idx].status = "DQ (Dropped)";
        for(int i=0; i<4; i++) {
            if(v_teams[idx].splits[i] == "-") v_teams[idx].splits[i] = "DQ";
        }
        events.push_back("[DISQUALIFIED] " + v_teams[idx].name + ": Dropped by " + player);
    }

    // ---------------------------------------------------------
    // CASE 4: WINNER ANNOUNCEMENT
    // ---------------------------------------------------------
    if (msg.find("WINNER") != string::npos) {
        v_teams[idx].Winner = true;
        
        // Only update status if we don't have the official time yet
        if (v_teams[idx].status.find("Official") == string::npos) {
            if (v_teams[idx].status.find("Finished") != string::npos) {
                v_teams[idx].status = "WINNER! " + v_teams[idx].status;
            } else {
                v_teams[idx].status = "WINNER! (Calculating...)";
            }
        }
    }

    // ---------------------------------------------------------
    // CASE 5: FINAL RESULT (OFFICIAL TIME)
    // Example Msg: "Final Result: Team [Name] finished in [Time] seconds"
    // ---------------------------------------------------------
    if (msg.find("Final Result: Team") != string::npos) {
        size_t startTime = msg.find("finished in ");
        size_t endTime = msg.find(" seconds");
        
        if (startTime != string::npos && endTime != string::npos) {
            string timeStr = msg.substr(startTime + 12, endTime - (startTime + 12));
            
            // Format time string to 3 decimal places
            try {
                double t = stod(timeStr);
                stringstream stream;
                stream << fixed << setprecision(3) << t;
                timeStr = stream.str();
            } catch(...) {}

            string resultStr = "Official Time: " + timeStr + "s";
            if (v_teams[idx].Winner) {
                v_teams[idx].status = "WINNER! " + resultStr;
            } else {
                v_teams[idx].status = resultStr;
            }
        }
    }
}

void UIdisp(string lastMsg) {
    clearScreen();
    cout << "====================================================================================\n";
    cout << "                         OLYMPIC RELAY RACE MONITOR (UDP)                           \n";
    cout << "====================================================================================\n";
    
    cout << left << setw(15) << "TEAM" 
         << "| " << setw(10) << "LEG 1" 
         << "| " << setw(10) << "LEG 2" 
         << "| " << setw(10) << "LEG 3" 
         << "| " << setw(10) << "LEG 4" 
         << "| " << setw(35) << "STATUS / FINAL TIME" << endl; 
    cout << "---------------+-----------+-----------+-----------+-----------+--------------------\n";

    for (const auto& t : v_teams) {
        cout << left << setw(15) << t.name 
             << "| " << setw(10) << t.splits[0] 
             << "| " << setw(10) << t.splits[1] 
             << "| " << setw(10) << t.splits[2] 
             << "| " << setw(10) << t.splits[3] 
             << "| " << setw(35) << t.status << endl;
    }
    cout << "====================================================================================\n";
    
    if (!events.empty()) {
        cout << "\n[ !!! FUMBLES & DISQUALIFICATIONS !!! ]\n";
        for (const string& evt : events) {
            cout << " > " << evt << endl;
        }
        cout << "------------------------------------------------------------------------------------\n";
    }

    cout << "\nLATEST LOG: " << lastMsg << endl;
}

int main() {
    try {
        // Setup ASIO for UDP
        asio::io_context io_context;
        udp::socket socket(io_context, udp::endpoint(udp::v4(), 3000)); // Listen on port 3000
        
        UIdisp("Waiting for race to start...");

        // Infinite loop to receive messages
        for (;;) {
            std::array<char, 1024> recv_buf;
            udp::endpoint remote_endpoint;
            
            // Block until data is received
            size_t len = socket.receive_from(asio::buffer(recv_buf), remote_endpoint);
            string msg(recv_buf.data(), len);
            
            // Clean up message (remove trailing newline)
            string clr_message = msg; 
            if (!clr_message.empty() && clr_message.back() == '\n') clr_message.pop_back();

            // Process the message and update the screen
            processMessage(msg);
            UIdisp(clr_message);
        }
    } 
    catch (std::exception& e) {
        std::cerr << "Monitor Exception: " << e.what() << "\n";
    }
    return 0;
}