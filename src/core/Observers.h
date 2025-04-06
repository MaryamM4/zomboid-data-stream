/**
 * Generic Observer object and storage.
 * Uses a parameter pack to generalize Observer's parameters.
 *
 * Example usage is commented at the very end.
 *
 * References:
 * https://stackoverflow.com/questions/70122423/what-is-typename-syntax-in-c-template
 * https://en.cppreference.com/w/cpp/language/parameter_pack
 * https://en.wikipedia.org/wiki/Observer_pattern#C++
 * https://stackoverflow.com/questions/2298242/observer-functions-in-c#28689902
 */

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace std;

/**
 *  Interface needed for storing Observers with different args togather.
 */
class Updatable {
public:
  virtual ~Updatable() = default;
  virtual void update() = 0; // This can't do anything on it's own.
};

/**
 * Template class for Observers.
 * Allows having different parameters for update(...).
 */
template <typename... Args> class Observer : public Updatable {
public:
  template <typename ObserverFunc>
  Observer(ObserverFunc observerFunc) : observerFunc(observerFunc) {}

  void update() override {
    // throw logic_error("Observer update must be called with parameters.");
    //  The Observer update function may not actually have arguments.
    update();
  }

  // update(...) with any numbers of args.
  void update(Args... args) { observerFunc(args...); }

protected:
  std::function<void(Args...)> observerFunc;
};

/**
 * Maps Observer(s) to each Subject codes.
 *
 * On an event, call notifyObservers(eventCode, args...)
 *
 * The notify function can't handle observers for
 * a single subject code have different parameters,
 * so register observers with that in mind.
 */
class Observers {
public:
  template <typename... Args>
  void registerObserver(uint8_t subjectCode,
                        std::function<void(Args...)> observerFunc) {
    auto observer = std::make_shared<Observer<Args...>>(observerFunc);
    observers[subjectCode].push_back(observer);
  }

  template <typename... Args>
  void notifyObservers(uint8_t subjectCode, Args... args) {
    // Find the Subject:
    auto it = observers.find(subjectCode);
    if (it != observers.end()) {
      // Iterate through subscribed Observers:
      for (const auto &observer : it->second) {
        auto typedObserver =
            std::dynamic_pointer_cast<Observer<Args...>>(observer);

        if (typedObserver) {
          typedObserver->update(args...);

        } else {
          cerr << "notifyObservers error: Observer parameter mismatch "
                  "(subjectCode: "
               << subjectCode << ")." << endl;
        }
      }
    }
  }

private:
  // Maps 1 Subject (code) to 1+ Observer(s) to notify on an event
  map<uint8_t, vector<std::shared_ptr<Updatable>>> observers;
};

/*
Example usage:

class NetworkAPI {
public:
    uint8_t moveSubjectCode = 'M';

    void registerObserver(uint8_t subjectCode, std::function<void(int, int)>
observer) { observers.registerObserver(subjectCode, observer);
    }

    void handleIncomingSubject(uint8_t subjectCode, int playerID, int move) {
        observers.notifyObservers(subjectCode, playerID, move);
    }

private:
    Observers observers;
};

int main() {
    NetworkAPI netAPI;

    auto onPlayerMove = [](int playerID, int move) {
        cout << "Player " << playerID << " made move " << move << endl;
    };

    netAPI.registerObserver(netAPI.moveSubjectCode, onPlayerMove);

    // Simulate an event:
    netAPI.handleIncomingSubject(netAPI.moveSubjectCode, 42, 1);

    return 0;
}
*/
