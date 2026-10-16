#include <string>


 // Program constants
namespace FrameAnalysis {

    const std::string PROGRAM_NAME = "3D Spatial Frame Analysis Program";
    const std::string VERSION = "1.0.0";
    const std::string BUILD_DATE = __DATE__;
    const std::string AUTHOR = "Structural Analysis Team";

    const std::string DEFAULT_INPUT_FILE = "input.txt";
    const std::string DEFAULT_OUTPUT_FILE = "output.txt";

    enum class ExitCode {
        SUCCESS = 0,
        FILE_ERROR = 1,
        PARSING_ERROR = 2,
        ANALYSIS_ERROR = 3,
        MEMORY_ERROR = 4,
        UNKNOWN_ERROR = 99
    };

    struct AnalysisOptions {
        bool verboseOutput = true;          // Enable detailed output
        bool saveResults = false;           // Save results to file
        bool validateInput = true;          // Validate input data
        double tolerance = 1e-12;           // Numerical tolerance
        std::string outputFileName;         // Output file name
    };
}