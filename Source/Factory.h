#pragma once
#include <unordered_map>

// https://stackoverflow.com/questions/1809670/how-to-implement-serialization-to-a-stream-for-a-class

/**
* A class for creating objects, with the type of object created based on a key
*
* @param K the key
* @param T the super class that all created classes derive from
*/
template<typename K, typename T>
class Factory {
private:
    typedef T* (*CreateObjectFunc)();

    /**
    * A map keys (K) to functions (CreateObjectFunc)
    * When creating a new type, we simply call the function with the required key
    */
    std::unordered_map<K, CreateObjectFunc> mObjectCreator;

    /**
    * Pointers to this function are inserted into the map and called when creating objects
    *
    * @param S the type of class to create
    * @return a object with the type of S
    */
    template<typename S>
    static T* createObject() {
        return new S();
    }

public:

    const std::unordered_map<K, CreateObjectFunc>& get_object_creator() const {
        return mObjectCreator;
    }

    /**
    * Registers a class to that it can be created via createObject()
    *
    * @param S the class to register, this must ve a subclass of T
    * @param id the id to associate with the class. This ID must be unique
    */
    template<typename S>
    void registerClass(K id) {
        if (mObjectCreator.find(id) != mObjectCreator.end()) {
            //your error handling here
        }

        mObjectCreator[id] = createObject<S>;
    }

    /**
    * Returns true if a given key exists
    *
    * @param id the id to check exists
    * @return true if the id exists
    */
    bool hasClass(K id) {
        return mObjectCreator.find(id) != mObjectCreator.end();
    }

    /**
    * Creates an object based on an id. It will return null if the key doesn't exist
    *
    * @param id the id of the object to create
    * @return the new object or null if the object id doesn't exist
    */
    T* createObject(K id) {
        //Don't use hasClass here as doing so would involve two lookups
        typename std::unordered_map<K, CreateObjectFunc>::iterator iter = mObjectCreator.find(id);
        if (iter == mObjectCreator.end()) {
            return NULL;
        }
        //calls the required createObject() function
        return ((*iter).second)();
    }
};
