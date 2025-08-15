// Complex nested structures plugin for cross-library testing
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <memory>
#include <variant>
#include <chrono>
#include <iostream>

// Forward declarations for circular references
struct Person;
struct Company;
struct Project;

// Basic nested structures
struct Address {
    std::string street;
    std::string city;
    std::string state;
    std::string country;
    int postal_code;
    std::optional<std::string> unit;
    
    std::string full_address() const {
        std::string result = street;
        if (unit.has_value()) {
            result += " " + unit.value();
        }
        return result + ", " + city + ", " + state + " " + std::to_string(postal_code) + ", " + country;
    }
    
    bool is_international() const {
        return country != "USA";
    }
};

struct ContactInfo {
    std::string email;
    std::string phone;
    std::optional<std::string> website;
    std::unordered_map<std::string, std::string> social_media;
    
    void add_social_media(const std::string& platform, const std::string& handle) {
        social_media[platform] = handle;
    }
    
    bool has_social_media(const std::string& platform) const {
        return social_media.find(platform) != social_media.end();
    }
};

// Complex nested structure with containers and circular references
struct Person {
    uint32_t id;
    std::string first_name;
    std::string last_name;
    int age;
    Address home_address;
    std::optional<Address> work_address;
    ContactInfo contact;
    std::vector<std::string> skills;
    std::map<std::string, double> skill_ratings; // skill -> rating (0.0-10.0)
    std::vector<uint32_t> friend_ids; // References to other Person IDs
    std::optional<uint32_t> manager_id;
    std::vector<uint32_t> direct_report_ids;
    
    // Methods
    std::string full_name() const {
        return first_name + " " + last_name;
    }
    
    void add_skill(const std::string& skill, double rating) {
        skills.push_back(skill);
        skill_ratings[skill] = rating;
    }
    
    double get_skill_rating(const std::string& skill) const {
        auto it = skill_ratings.find(skill);
        return it != skill_ratings.end() ? it->second : 0.0;
    }
    
    size_t friend_count() const {
        return friend_ids.size();
    }
    
    bool has_manager() const {
        return manager_id.has_value();
    }
    
    size_t direct_report_count() const {
        return direct_report_ids.size();
    }
    
    std::string contact_summary() const {
        return full_name() + " (" + contact.email + ")";
    }
};

// Even more complex structure with deep nesting
struct Project {
    uint32_t id;
    std::string name;
    std::string description;
    std::vector<uint32_t> team_member_ids;
    uint32_t project_manager_id;
    std::map<std::string, std::variant<std::string, int, double, bool>> metadata;
    std::vector<std::string> tags;
    
    // Nested timeline structure
    struct Milestone {
        std::string name;
        std::string description;
        std::string due_date; // ISO date string
        bool completed;
        std::vector<std::string> deliverables;
        std::optional<std::string> completion_date;
        
        void complete(const std::string& date) {
            completed = true;
            completion_date = date;
        }
        
        bool is_overdue(const std::string& current_date) const {
            return !completed && current_date > due_date;
        }
    };
    
    std::vector<Milestone> milestones;
    
    // Methods
    void add_team_member(uint32_t person_id) {
        team_member_ids.push_back(person_id);
    }
    
    size_t team_size() const {
        return team_member_ids.size();
    }
    
    void add_milestone(const std::string& name, const std::string& desc, const std::string& due_date) {
        Milestone m;
        m.name = name;
        m.description = desc;
        m.due_date = due_date;
        m.completed = false;
        milestones.push_back(m);
    }
    
    size_t completed_milestones() const {
        size_t count = 0;
        for (const auto& m : milestones) {
            if (m.completed) count++;
        }
        return count;
    }
    
    double completion_percentage() const {
        if (milestones.empty()) return 0.0;
        return (double)completed_milestones() / milestones.size() * 100.0;
    }
};

struct Company {
    uint32_t id;
    std::string name;
    std::string industry;
    Address headquarters;
    std::vector<Address> office_locations;
    std::vector<Person> employees;
    std::vector<Project> projects;
    ContactInfo company_contact;
    
    // Nested department structure
    struct Department {
        std::string name;
        uint32_t head_id; // Person ID
        std::vector<uint32_t> employee_ids;
        std::string budget_code;
        
        size_t size() const {
            return employee_ids.size();
        }
        
        void add_employee(uint32_t person_id) {
            employee_ids.push_back(person_id);
        }
    };
    
    std::vector<Department> departments;
    std::unordered_map<std::string, std::string> company_metadata;
    
    // Methods
    void add_employee(const Person& person) {
        employees.push_back(person);
    }
    
    void add_project(const Project& project) {
        projects.push_back(project);
    }
    
    size_t employee_count() const {
        return employees.size();
    }
    
    size_t project_count() const {
        return projects.size();
    }
    
    std::vector<Person> find_employees_with_skill(const std::string& skill) const {
        std::vector<Person> result;
        for (const auto& emp : employees) {
            if (emp.skill_ratings.find(skill) != emp.skill_ratings.end()) {
                result.push_back(emp);
            }
        }
        return result;
    }
    
    Project* find_project_by_name(const std::string& name) {
        for (auto& proj : projects) {
            if (proj.name == name) return &proj;
        }
        return nullptr;
    }
    
    std::string company_summary() const {
        return name + " (" + std::to_string(employee_count()) + " employees, " + 
               std::to_string(project_count()) + " projects)";
    }
};

// Global instances for testing complex structures
Company global_tech_company;
Person global_ceo;
Project global_flagship_project;

// C API functions for testing complex structure passing
extern "C" {
    // Company operations
    void* create_company() {
        Company* company = new Company{};  // Use brace initialization
        company->id = 1001;
        company->name = "TechCorp Global";
        company->industry = "Software Development";
        
        
        // Set headquarters
        company->headquarters = {
            "123 Innovation Drive",
            "San Francisco",
            "CA",
            "USA",
            94105,
            "Suite 500"
        };
        
        // Add office locations
        company->office_locations.push_back({
            "456 Tech Avenue",
            "Seattle",
            "WA", 
            "USA",
            98101,
            std::nullopt
        });
        
        company->office_locations.push_back({
            "789 Digital Street",
            "Austin",
            "TX",
            "USA",
            73301,
            std::nullopt
        });
        
        return company;
    }
    
    void delete_company(void* company_ptr) {
        delete static_cast<Company*>(company_ptr);
    }
    
    // Person operations
    void* create_person(uint32_t id, const char* first_name, const char* last_name, int age) {
        Person* person = new Person();
        person->id = id;
        person->first_name = first_name;
        person->last_name = last_name;
        person->age = age;
        
        // Set home address
        person->home_address = {
            "321 Residential Lane",
            "Palo Alto",
            "CA",
            "USA",
            94301,
            "Apt 2B"
        };
        
        // Set contact info
        person->contact.email = std::string(first_name) + "." + last_name + "@example.com";
        person->contact.phone = "+1-555-0123";
        person->contact.add_social_media("linkedin", std::string(first_name) + last_name);
        
        return person;
    }
    
    void delete_person(void* person_ptr) {
        delete static_cast<Person*>(person_ptr);
    }
    
    // Project operations
    void* create_project(uint32_t id, const char* name, const char* description) {
        Project* project = new Project();
        project->id = id;
        project->name = name;
        project->description = description;
        project->project_manager_id = 1001; // Default manager
        
        // Add some milestones
        project->add_milestone("Phase 1: Planning", "Initial project planning and setup", "2024-03-01");
        project->add_milestone("Phase 2: Development", "Core development work", "2024-06-01");
        project->add_milestone("Phase 3: Testing", "Quality assurance and testing", "2024-08-01");
        project->add_milestone("Phase 4: Deployment", "Production deployment", "2024-09-01");
        
        // Add metadata
        project->metadata["budget"] = 500000;
        project->metadata["priority"] = std::string("high");
        project->metadata["confidential"] = true;
        project->metadata["completion_bonus"] = 15.5;
        
        return project;
    }
    
    void delete_project(void* project_ptr) {
        delete static_cast<Project*>(project_ptr);
    }
    
    // Structure access functions
    const char* get_person_full_name(void* person_ptr) {
        auto* person = static_cast<Person*>(person_ptr);
        static thread_local std::string result;
        result = person->full_name();
        return result.c_str();
    }
    
    const char* get_person_email(void* person_ptr) {
        auto* person = static_cast<Person*>(person_ptr);
        return person->contact.email.c_str();
    }
    
    const char* get_person_home_address(void* person_ptr) {
        auto* person = static_cast<Person*>(person_ptr);
        static thread_local std::string result;
        result = person->home_address.full_address();
        return result.c_str();
    }
    
    void add_person_skill(void* person_ptr, const char* skill, double rating) {
        auto* person = static_cast<Person*>(person_ptr);
        person->add_skill(skill, rating);
    }
    
    double get_person_skill_rating(void* person_ptr, const char* skill) {
        auto* person = static_cast<Person*>(person_ptr);
        return person->get_skill_rating(skill);
    }
    
    const char* get_company_name(void* company_ptr) {
        auto* company = static_cast<Company*>(company_ptr);
        return company->name.c_str();
    }
    
    const char* get_company_headquarters_address(void* company_ptr) {
        auto* company = static_cast<Company*>(company_ptr);
        static thread_local std::string result;
        result = company->headquarters.full_address();
        return result.c_str();
    }
    
    size_t get_company_office_count(void* company_ptr) {
        auto* company = static_cast<Company*>(company_ptr);
        return company->office_locations.size();
    }
    
    void add_employee_to_company(void* company_ptr, void* person_ptr) {
        auto* company = static_cast<Company*>(company_ptr);
        auto* person = static_cast<Person*>(person_ptr);
        company->add_employee(*person);
    }
    
    size_t get_company_employee_count(void* company_ptr) {
        auto* company = static_cast<Company*>(company_ptr);
        return company->employees.size();  // Use the vector size directly
    }
    
    const char* get_project_name(void* project_ptr) {
        auto* project = static_cast<Project*>(project_ptr);
        return project->name.c_str();
    }
    
    size_t get_project_milestone_count(void* project_ptr) {
        auto* project = static_cast<Project*>(project_ptr);
        return project->milestones.size();
    }
    
    double get_project_completion_percentage(void* project_ptr) {
        auto* project = static_cast<Project*>(project_ptr);
        return project->completion_percentage();
    }
    
    void complete_project_milestone(void* project_ptr, size_t milestone_index, const char* completion_date) {
        auto* project = static_cast<Project*>(project_ptr);
        if (milestone_index < project->milestones.size()) {
            project->milestones[milestone_index].complete(completion_date);
        }
    }
    
    void add_project_to_company(void* company_ptr, void* project_ptr) {
        auto* company = static_cast<Company*>(company_ptr);
        auto* project = static_cast<Project*>(project_ptr);
        company->add_project(*project);
    }
    
    // Complex nested access
    const char* get_employee_skill_at_index(void* company_ptr, size_t employee_index, size_t skill_index) {
        auto* company = static_cast<Company*>(company_ptr);
        if (employee_index < company->employees.size() && 
            skill_index < company->employees[employee_index].skills.size()) {
            static std::string result = company->employees[employee_index].skills[skill_index];
            return result.c_str();
        }
        return nullptr;
    }
    
    const char* get_project_milestone_name(void* company_ptr, size_t project_index, size_t milestone_index) {
        auto* company = static_cast<Company*>(company_ptr);
        if (project_index < company->projects.size() && 
            milestone_index < company->projects[project_index].milestones.size()) {
            static std::string result = company->projects[project_index].milestones[milestone_index].name;
            return result.c_str();
        }
        return nullptr;
    }
    
    // Plugin info
    const char* plugin_name() {
        return "ComplexStructuresPlugin";
    }
    
    const char* plugin_version() {
        return "2.0.0";
    }
    
    const char* plugin_description() {
        return "Plugin for testing complex nested C++ structures across shared library boundaries";
    }
}