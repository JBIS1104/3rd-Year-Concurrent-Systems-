// 10936972_Park_Task1to3_v6.cpp
// Rename your file to be   StudentID_FamilyName_vVersion.cpp
// And put that information in the file header
//TODO  StudentID = 10936972
//TODO  FamilyName = Park

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#include "asio.hpp"//Add ASIO library for UDP communication (task 4)
#include <iostream>
#include <string>   // text manipulation
#include <thread>   // threading support
#include <mutex>    //mutex is needed for thread safety
#include <random>   // For std::uniform_real_distribution, std::mt19937, and std::random_device
#include <condition_variable> // for task 3 baton passing
#include <atomic> 
#include <chrono>   // time measurement
#include "barrier.hpp"  //Custom barrier class
#include "cs_helper_DoNotModify.hpp" //helper classes provided for the assignment

using namespace std;    
using asio::ip::udp;    // For UDP communication (Task 4)

const int NUM_TEAMS = 4;      // number of teams in the race
const int NUM_MEMBERS = 4;    // number of athletes in the team

// UDP Global Variables for Task 4
asio::io_context UDP_IO;               //manage I/O for UDP
udp::endpoint reciever_addr;           //store receiver address
udp::socket* udpsocket_ptr = nullptr;  //pointer to UDP socket, used to globally send messages

// Data for team/athelete initialisation. 
//The Women’s 4x100 meter relay at the Tokyo 2020 Olympics. The teams took between 41 and 42 seconds.
std::array< string, 4> astrTeams = { "Jamaica", "United States", "Great Britain", "Switzerland" };
std::array< std::array<std::string, 4>, 4> aastrCompetitors = {{
    { "Williams", "Thompson-Herah", "Fraser-Pryce", "Jackson" },
    { "Oliver", "Daniels", "Prandini", "Thomas" },
    { "Philip", "Lansiquot", "Asher-Smith", "Neita" },
    { "Del-Ponte", "Kambundji", "Kora", "Dietsche" }
}};

class RandomTwister{                                                         // Thread-safe random number generator class
public:
    RandomTwister(float min, float max) : distribution(min, max) {}    // Initialises uniform_real_distribution to initialize to a specific range of numbers
    float generate()                                                                                 // Returns a random float within the specified range
    {
        //Part 1.1 Make the Random number generator thread-safe by adding a simple std::mutex,
        // and rembember the unlock!  Instantiate the mutex into private: area of this class, below
        std::lock_guard<std::mutex> lock(mtx); // Lock the mutex for thread safety, lock is automatically released when going out of scope
        return distribution(engine);           // Generate and return the random number using mt19937
    }
private:
    // std::random_device creates a seed value for the mt19937 instance creation. It creates a seed value for the “mt” random number generator
    std::mt19937 engine{std::random_device{}()};            // Mersenne Twister random number generator engine, with a seed from random_device() - static
    std::uniform_real_distribution<float> distribution;     // This uniform_real_distribution transforms the engine output into the required (min, max) range and data type.
    std::mutex mtx;                                         // Mutex to protect engine and distribution
};


//Part 1.2 Make thrd_print thread safe.  Instantiate a mutex here (global as it is shared between threads) and use it to protect the function using a std::lock_guard<std::mutex>
std::mutex print_mtx;                                       // instantiating Mutex for thread-safe printing
void thrd_print(const std::string& str) {                   // Thread safe print
    std::lock_guard<std::mutex> lock(print_mtx);            // lock mutex for thread safety, automatically released when going out of scope
    std::cout << str << std::flush;                         // Print the message (Added flush)
    if (udpsocket_ptr) {                                   // check is the network  is set up
        try { udpsocket_ptr->send_to(asio::buffer(str), reciever_addr); } catch (...) {} // Sends the text to UDP receiver
    }
}

barrier barrier_allthreads_started(1+(NUM_TEAMS * NUM_MEMBERS)); // Need all the thread to reach here before the start can continue.

//Part 1.3 Create another barrier array and name it "barrier_go" which you will use to make all threads wait until the race official starts the race
barrier barrier_go(1+(NUM_TEAMS * NUM_MEMBERS)); // all thread waiting for gunshot

//Part 2.1  Create a std::atomic variable of type bool, initalised to false and name it "winner". You will use it to ensure just the winning thread claims to have won the race.
std::atomic<bool> winner(false); // atomic flag to indicate the winning team, atomic ensure only the first to set it wins

// Used for Task 3 
std::atomic<bool> Disqualified[NUM_TEAMS]; // atomic flags to indicate if a team is disqualified

void thd_runner_4x4x100m(Competitor& a, Competitor *pPrevA, RandomTwister& generator, int teamIdx, RandomTwister& probGen) {
    thrd_print(a.getPerson() + " ready, "); //prints the athlete is ready
    
    //Part 2.2 Copy the code from thd_runner_16x100m for barriers

    //Copied code from Part 1.4 Apply the barrier_allthreads_started using arrive_and_wait().
    barrier_allthreads_started.arrive_and_wait();// Wait at the barrier until all threads are running

    //Copied code from Part 1.5 Apply the barrier_go using arrive_and_wait().
    barrier_go.arrive_and_wait();
    
    if ( pPrevA == NULL) {
        // First runner starts immediately
        thrd_print(a.getPerson() + " started (Running...)\n"); // Added newline and running status
    }
    else { // other runners waiting for baton
            { 
                //Part 2.3 Create a std::unique_lock<std::mutex> called "lock", initialised with pPrevA->mtx mutex
                std::unique_lock<std::mutex> lock(pPrevA->mtx); // locks previous runner's mutex

                //Part 2.4 Complete the pPrevA->baton condition_variable line below to wait on that lock.
                pPrevA->baton.wait(lock, [&] { return pPrevA->bFinished; }); // waits until previous runner has finished 
            }

            if(Disqualified[teamIdx]) { // Task 3: Stop subsequent runners if DQ'd
                { 
                std::lock_guard<std::mutex> lock(a.mtx);    // lock mutex
                a.bFinished = true;                         // mark the runner finished so next runner doesn't wait
                a.baton.notify_all();                       // notify any waiting teammates
                }
                return; // disqualified team exit the function
            }
            thrd_print( a.getPerson() +" ("+ a.getTeamName() + ")" +" took the baton from " + pPrevA->getPerson() +" ("+pPrevA->getTeamName() + ")\n");
        }

        // Task 3: For baton drop and fumble
        if (pPrevA != nullptr) {

            float baton_percentage = probGen.generate();  // generate random chance using probability generator

            if (baton_percentage < 0.05f) { // Drop (< 5%)
                thrd_print("\n No Way! The Baton Dropped From " + a.getPerson() + " ! Team " + a.getTeamName() + " is now DISQUALIFIED!\n\n");
                Disqualified[teamIdx] = true; // mark team as disqualified
                { std::lock_guard<std::mutex> lock(a.mtx);
                a.bFinished = true;         // mark the runner finished so next runner doesn't wait
                a.baton.notify_all(); }     // notify any waiting teammates
                return; 
            }
            else if (baton_percentage < 0.20f) { // Fumble (5% to 20%)
                float fumble_delay = baton_percentage * 10.0f; // Delay between 0.5s to 2s
                thrd_print("\n Oh No! Baton Fumbled by " + a.getPerson() + " ! The team " + a.getTeamName() + " is delayed by: " + std::to_string(fumble_delay) + "s \n");
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(fumble_delay * 1000))); // simulate delay
            }
        }
    

    float fSprintDuration_seconds = generator.generate(); // Generate sprint duration
    //Part 2.5 Copy the code from thd_runner_16x100m for fSprintDuration_seconds and std::this_thread::sleep_for

    //Copied code from Part 1.6
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(fSprintDuration_seconds * 1000))); // simulate sprint
    a.setTime(fSprintDuration_seconds); // record the sprint time
    thrd_print( "Leg "+ std::to_string(a.numBatonExchanges()) + ": "+a.getPerson() + " ran in " + std::to_string(fSprintDuration_seconds) + " seconds. ("+ a.getTeamName() + ")\n");

    // Baton Pass
    {
    std::lock_guard<std::mutex> lock(a.mtx); // lock own mutex
    a.bFinished = true;                      // set signal flag finished
    a.baton.notify_all();                    // notify next runner
    }

    if ( a.numBatonExchanges() == NUM_MEMBERS) { // Last runner
        //Part 2.6 Use an atomic .exchange on the atomic "winner" object
        if (!Disqualified[teamIdx] && !winner.exchange(true)) { // if team not disqualified and winner flag was false
            string winMsg = "\n Team " + a.getTeamName() + " is the WINNER!\n";
            thrd_print(winMsg); // Changed to thrd_print to ensure UDP gets it too
        }
    }
}

int main() {
    // UDP/ASIO initialisation for Task 4
    udp::socket socket(UDP_IO); // create UDP socket
    socket.open(udp::v4());     // open the socket for IPv4
    udpsocket_ptr = &socket;    // set global pointer to the socket
    reciever_addr = udp::endpoint(asio::ip::address::from_string("127.0.0.1"), 3000);   // set receiver address to localhost:3000

    thread     thread_competitor[NUM_TEAMS][NUM_MEMBERS];  // 2D array of threads.
    Team aTeams[NUM_TEAMS]; // Can be Global               // array of Teams.
    Competitor athlete[NUM_TEAMS][NUM_MEMBERS];            // 2D array of Competitors.
    float afTeamTime_s[NUM_TEAMS];                         // array to store team times
    // Start threads in each position of the 2D array
    for(int i=0; i<NUM_TEAMS; ++i) Disqualified[i] = false;// initialize disqualification flags
    
    // Part 1.7   Change the random number generation to between 10 s and 12 s.  (you might want to do this later so you don't have to wait while you are debugging!)
    RandomTwister randGen_sprint_time(10.0f, 12.0f);        // Random generator, 10 to 12 seconds
    RandomTwister probabilityGen(0.0f, 1.0f);               // Random generator, 0.0 to 1.0    
    std::cout << "Re-run of the women's 4x100 meter relay at the Tokyo 2020 Olympics.\n" << std::endl;
    
    for (int i = 0; i < NUM_TEAMS; ++i) {
        afTeamTime_s[i] = 0;                     // initialize
        aTeams[i].setTeam(astrTeams[i]);             // Sets the team name
        for (int j = 0; j < NUM_MEMBERS; ++j) {      // For each team member
            athlete[i][j].set(aastrCompetitors[i][j], &(aTeams[i]) );  // Create the athlete information
            
            // thd_runner_16x100m( std::ref(athlete[i][j]), std::ref(randGen_sprint_time) );
            
// THIS IS FOR PART 2
            //Part 2.7 Start the thd_runner_4x4x100m instead. If it is the first runner of the team (j==0) then Competitor *pPrevA should be NULL, otherwide it should be the previous runner (Competitor *)&(athlete[i][j-1])
            if ( j==0 ) thread_competitor[i][j] = std::thread(thd_runner_4x4x100m, std::ref(athlete[i][j]), nullptr, std::ref(randGen_sprint_time), i, std::ref(probabilityGen)); // First runner in team
            else thread_competitor[i][j] = std::thread(thd_runner_4x4x100m, std::ref(athlete[i][j]), (Competitor*)&athlete[i][j-1], std::ref(randGen_sprint_time), i, std::ref(probabilityGen)); // Passed baton
        }
    }

    // Wait for all threads to be running including the main thread
    //Part 1.9 Apply the barrier_allthreads_started arrive_and_wait here to wait for all threads to be created (16 threads + this main thread = 17)
    barrier_allthreads_started.arrive_and_wait(); // Wait at the barrier until all threads arrive
    thrd_print("\n\nThe race official raises her starting pistol...\n");
    
    //Part 2.8 Change this starter gun time from the fixed 3.5 seconds (next line) to a random value between 3 to 5 seconds. 
    RandomTwister gunTimer(3.0f, 5.0f);              // Random generator, 3 to 5 seconds
    float fStarterGun_s = gunTimer.generate();          // generate random starter gun time
    
    //Part 1.10 Sleep using std::this_thread::sleep_for function for the fStarterGun_s  (see advice on type conversion given above)
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(fStarterGun_s * 1000))); // Sleep for starter gun time
    
    //Part 1.11  Apply the final barrier_go arrive_and_wait here to start all the competitors running.
    barrier_go.arrive_and_wait(); // Wait at the barrier until all threads arrive
    thrd_print("\nGO !\n\n");     
    
    // Join all threads
    for (int i = 0; i < NUM_TEAMS; ++i) {           // Loops through each team
        for (int j = 0; j < NUM_MEMBERS; ++j) {     // Loops through each team member
            //Part 1.12  For all thread_competitor[i][j], test the thread is joinable, and if so, join it
            if(thread_competitor[i][j].joinable()) thread_competitor[i][j].join();
        }
    }
    
    // Print the results for each team
    thrd_print("\n\nTEAM RESULTS\n");
    for (int i = 0; i < NUM_TEAMS; ++i){
        if(Disqualified[i]) {
            thrd_print("Team " + astrTeams[i] + " = DISQUALIFIED\n"); // print for disqualified teams
        }
        else {
            aTeams[i].printTimes(); // print for qualified teams, total time run time (local)
            
            // Calculate total time for UDP
            float totalRunT = 0.0f;
            for(int j = 0; j < NUM_MEMBERS; ++j) {
                totalRunT += athlete[i][j].getTime(); 
            }
            // Send formatted message
            string finalResult = "Final Result: Team " + astrTeams[i] + " finished in " + to_string(totalRunT) + " seconds\n";
            thrd_print(finalResult);
        }
    }
    thrd_print("\n");     // exit print
    return 0;
}