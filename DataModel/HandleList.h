#ifndef HANDLE_LIST_H
#define HANDLE_LIST_H

#include <list>

// A container for storing items that can later be deleted. A stored item has a
// handle that it can be accessed through. std::list::iterator is inappropriate
// for a handle since there is no way to check whether it is initialized.
template <typename T>
class HandleList {
  public:
    class Handle {
      public:
        Handle() {};

        void erase() {
          list->erase(iterator);
          list = nullptr;
        };

        operator bool() {
          return list;
        }

      private:
        Handle(std::list<T>& list, typename std::list<T>::iterator iterator):
          list(&list), iterator(iterator)
        {};

        std::list<T>* list;
        typename std::list<T>::iterator iterator;

        friend class HandleList;
    };

    typename std::list<T>::iterator begin() {
      return list.begin();
    }

    typename std::list<T>::iterator end() {
      return list.end();
    }

    Handle add(T item) {
      return Handle(list, list.insert(list.end(), std::move(item)));
    }

  private:
    std::list<T> list;
};

#endif
