#pragma once

namespace fast_io::containers {
    template<::std::movable T, typename allocator>
    class deque;

    namespace details {
        template<typename A>
        concept mini_alloc = requires(A &a)
        {
            typename A::value_type;
            a.allocate(0);
        };

#if not defined(__cpp_pack_indexing)
        template<typename Tuple>
        inline constexpr auto get_first_two(Tuple args) noexcept {
            auto &first = ::std::get<0>(args);
            auto &second = ::std::get<1>(args);
            struct iter_ref_pair {
                decltype(first) &begin;
                decltype(second) &end;
            };
            return iter_ref_pair{first, second};
        }
#else
#if defined(__cpp_pack_indexing) // make clang happy
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++26-extensions"
#endif
        template<typename... Args>
        inline constexpr auto get_first_two(Args &&... args) noexcept {
            auto &first = args...[0];
            auto &second = args...[1];
            struct iter_ref_pair {
                decltype(first) &begin;
                decltype(second) &end;
            };
            return iter_ref_pair{first, second};
        }
#if defined(__cpp_pack_indexing)
#pragma clang diagnostic pop
#endif
#endif


        template<typename T>
        inline constexpr ::std::size_t block_elements_v = 16 > 4096 / sizeof(T) ? 16 : 4096 / sizeof(T);

        template<typename T>
        inline constexpr auto to_address(T const t) noexcept {
            return ::std::to_address(t);
        }

        inline constexpr auto to_address(::std::nullptr_t) noexcept {
            return nullptr;
        }


        template<typename T>
        inline constexpr auto calc_cap(::std::size_t const size) noexcept {
            auto const block_elems = block_elements_v<T>;
            struct cap_t {
                ::std::size_t block_size;
                ::std::size_t full_blocks;
                ::std::size_t rem_elems;
            };
            return cap_t{(size + block_elems - 1) / block_elems, size / block_elems, size % block_elems};
        }


        template<typename T>
        inline constexpr auto calc_pos(::std::ptrdiff_t const front_size, ::std::ptrdiff_t const pos) noexcept {
            ::std::ptrdiff_t const block_elems = block_elements_v<T>;
            struct pos_t {
                ::std::ptrdiff_t block_step;
                ::std::ptrdiff_t elem_step;
            };
            if (pos >= 0) {
                auto const new_pos = pos + front_size;
                return pos_t{new_pos / block_elems, new_pos % block_elems};
            } else {
                auto const new_pos = pos + front_size - block_elems + 1;
                return pos_t{new_pos / block_elems, new_pos % block_elems - 1 + block_elems};
            }
        }

        template<typename T>
        inline constexpr auto calc_pos(::std::size_t const front_size, ::std::size_t const pos) noexcept {
            ::std::size_t const block_elems = block_elements_v<T>;
            struct pos_t {
                ::std::size_t block_step;
                ::std::size_t elem_step;
            };
            auto const new_pos = pos + front_size;
            return pos_t{new_pos / block_elems, new_pos % block_elems};
        }

        template<typename T, typename Block>
        class deque_iterator {
            using RConstT = ::std::remove_const_t<T>;

            template<::std::movable U, typename Allocator>
            friend class ::fast_io::containers::deque;
            friend deque_iterator<RConstT, Block>;
            friend deque_iterator<T const, Block>;

            Block *block_elem_curr_{};
            Block *block_elem_end_{};
            RConstT *elem_begin_{};
            RConstT *elem_curr_{};

            template<typename U, typename V>
            constexpr deque_iterator(Block *block_curr, Block *block_end, U const begin, V const pos) noexcept
                : block_elem_curr_(block_curr), block_elem_end_(block_end), elem_begin_(details::to_address(begin)),
                  elem_curr_(details::to_address(pos)) {
            }

            constexpr deque_iterator<RConstT, Block> remove_const_() const noexcept
                requires(::std::is_const_v<T>) {
                return {block_elem_curr_, block_elem_end_, elem_begin_, elem_curr_};
            }

            constexpr deque_iterator &plus_and_assign_(::std::ptrdiff_t const pos) noexcept {
                if (pos != 0) {
                    auto const [block_step, elem_step] = details::calc_pos<T>(elem_curr_ - elem_begin_, pos);
                    auto const target_block = block_elem_curr_ + block_step;
                    if (target_block < block_elem_end_) {
                        block_elem_curr_ = target_block;
                        elem_begin_ = ::std::to_address(*target_block);
                        elem_curr_ = elem_begin_ + elem_step;
                    } else {
                        if (!(target_block == block_elem_end_)) [[unlikely]] {
                            fast_terminate();
                        }
                        if (!(elem_step == 0)) [[unlikely]] {
                            fast_terminate();
                        }
                        block_elem_curr_ = target_block - 1;
                        elem_begin_ = ::std::to_address(*(target_block - 1));
                        elem_curr_ = elem_begin_ + details::block_elements_v<T>;
                    }
                }
                return *this;
            }

        public:
            using difference_type = ::std::ptrdiff_t;
            using value_type = T;
            using pointer = T *;
            using reference = T &;
            using iterator_category = ::std::random_access_iterator_tag;

            constexpr deque_iterator() noexcept = default;

            constexpr deque_iterator(deque_iterator const &other) noexcept = default;

            constexpr deque_iterator &operator=(deque_iterator const &other) noexcept = default;

            constexpr ~deque_iterator() = default;

            constexpr bool operator==(deque_iterator const &other) const noexcept {
                return elem_curr_ == other.elem_curr_;
            }

            constexpr pointer operator->() noexcept {
                if (!(elem_curr_ != elem_begin_ + details::block_elements_v<T>)) [[unlikely]] {
                    fast_terminate();
                }
                return elem_curr_;
            }

            constexpr pointer operator->() const noexcept {
                if (!(elem_curr_ != elem_begin_ + details::block_elements_v<T>)) [[unlikely]] {
                    fast_terminate();
                }
                return elem_curr_;
            }

            constexpr ::std::strong_ordering operator<=>(deque_iterator const &other) const noexcept {
                if (!(block_elem_end_ == other.block_elem_end_)) [[unlikely]] {
                    fast_terminate();
                }
                if (block_elem_curr_ < other.block_elem_curr_)
                    return ::std::strong_ordering::less;
                if (block_elem_curr_ > other.block_elem_curr_)
                    return ::std::strong_ordering::greater;
                if (elem_curr_ < other.elem_curr_)
                    return ::std::strong_ordering::less;
                if (elem_curr_ > other.elem_curr_)
                    return ::std::strong_ordering::greater;
                return ::std::strong_ordering::equal;
            }

            constexpr T &operator*() noexcept {
                if (!(elem_curr_ != elem_begin_ + details::block_elements_v<T>)) [[unlikely]] {
                    fast_terminate();
                }
                return *elem_curr_;
            }

            constexpr T &operator*() const noexcept {
                if (!(elem_curr_ != elem_begin_ + details::block_elements_v<T>)) [[unlikely]] {
                    fast_terminate();
                }
                return *elem_curr_;
            }
#if __has_cpp_attribute(__gnu__::__always_inline__)
            [[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
            [[msvc::forceinline]]
#endif
            constexpr deque_iterator &operator++() noexcept {
                if (!(elem_curr_ != elem_begin_ + details::block_elements_v<T>)) [[unlikely]] {
                    fast_terminate();
                }
                ++elem_curr_;
                if (elem_curr_ == elem_begin_ + details::block_elements_v<T>) {
                    if (block_elem_curr_ + 1 != block_elem_end_) {
                        ++block_elem_curr_;
                        elem_begin_ = ::std::to_address(*block_elem_curr_);
                        elem_curr_ = elem_begin_;
                    }
                }
                return *this;
            }
#if __has_cpp_attribute(__gnu__::__always_inline__)
            [[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
            [[msvc::forceinline]]
#endif
            constexpr deque_iterator operator++(int) noexcept {
#if defined(__cpp_auto_cast)
                return ++auto{*this};
#else
                auto temp(*this);
                ++temp;
                return temp;
#endif
            }
#if __has_cpp_attribute(__gnu__::__always_inline__)
            [[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
            [[msvc::forceinline]]
#endif
            constexpr deque_iterator &operator--() noexcept {
                if (elem_curr_ == elem_begin_) {
                    --block_elem_curr_;
                    elem_begin_ = ::std::to_address(*block_elem_curr_);
                    elem_curr_ = elem_begin_ + details::block_elements_v<T>;
                }
                --elem_curr_;
                return *this;
            }

#if __has_cpp_attribute(__gnu__::__always_inline__)
            [[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
            [[msvc::forceinline]]
#endif
            constexpr deque_iterator operator--(int) noexcept {
#if defined(__cpp_auto_cast)
                return --auto{*this};
#else
                auto temp(*this);
                --temp;
                return temp;
#endif
            }
#if __has_cpp_attribute(__gnu__::__always_inline__)
            [[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
            [[msvc::forceinline]]
#endif
            constexpr T &operator[](::std::ptrdiff_t const pos) noexcept {
                auto const [block_step, elem_step] = details::calc_pos<T>(elem_curr_ - elem_begin_, pos);
                auto const target_block = block_elem_curr_ + block_step;
                if (!(target_block < block_elem_end_)) [[unlikely]] {
                    fast_terminate();
                }
                return *((*target_block) + elem_step);
            }
#if __has_cpp_attribute(__gnu__::__always_inline__)
            [[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
            [[msvc::forceinline]]
#endif
            constexpr T &operator[](::std::ptrdiff_t const pos) const noexcept {
                auto const [block_step, elem_step] = details::calc_pos<T>(elem_curr_ - elem_begin_, pos);
                auto const target_block = block_elem_curr_ + block_step;
                if (!(target_block < block_elem_end_)) [[unlikely]] {
                    fast_terminate();
                }
                return *((*target_block) + elem_step);
            }

            friend constexpr ::std::ptrdiff_t operator-(deque_iterator const &lhs, deque_iterator const &rhs) noexcept {
                if (!(lhs.block_elem_end_ == rhs.block_elem_end_)) [[unlikely]] {
                    fast_terminate();
                }
                auto const block_size = lhs.block_elem_curr_ - rhs.block_elem_curr_;
                return block_size * static_cast<::std::ptrdiff_t>(block_elements_v<T>) + lhs.elem_curr_ - lhs.
                       elem_begin_ -
                       (rhs.elem_curr_ - rhs.elem_begin_);
            }

            constexpr deque_iterator &operator+=(::std::ptrdiff_t const pos) noexcept {
                return plus_and_assign_(pos);
            }

            friend constexpr deque_iterator operator+(deque_iterator const &it, ::std::ptrdiff_t const pos) noexcept {
#if defined(__cpp_auto_cast)
                return auto{it}.plus_and_assign_(pos);
#else
                auto temp = it;
                temp.plus_and_assign_(pos);
                return temp;
#endif
            }

            friend constexpr deque_iterator operator+(::std::ptrdiff_t const pos, deque_iterator const &it) noexcept {
                return it + pos;
            }

            constexpr deque_iterator &operator-=(::std::ptrdiff_t const pos) noexcept {
                return plus_and_assign_(-pos);
            }

            friend constexpr deque_iterator operator-(deque_iterator const &it, ::std::ptrdiff_t const pos) noexcept {
                return it + (-pos);
            }

            friend constexpr deque_iterator operator-(::std::ptrdiff_t const pos, deque_iterator const &it) noexcept {
                return it + (-pos);
            }

            constexpr operator deque_iterator<T const, Block>() const
                requires(not::std::is_const_v<T>) {
                return {block_elem_curr_, block_elem_end_, elem_begin_, elem_curr_};
            }
        };
    }

    template<::std::movable T, typename allocator>
    class deque {
        using typed_allocator_type = typed_generic_allocator_adapter<allocator, T>;
        using typed_allocator_pointer_type = typed_generic_allocator_adapter<allocator, T *>;

        static constexpr bool is_default_operation_ = requires(allocator &a)
        {
            a.construct(static_cast<T *>(nullptr));
        };


        static inline constexpr ::std::size_t block_elements = details::block_elements_v<T>;

        using Block = T *;
        using BlockFP = Block *;


        BlockFP block_ctrl_begin_fancy_{};

        Block *block_ctrl_end_{};

        Block *block_alloc_begin_{};

        Block *block_alloc_end_{};

        Block *block_elem_begin_{};

        Block *block_elem_end_{};

        T *elem_begin_first_{};

        T *elem_begin_begin_{};

        T *elem_begin_end_{};

        T *elem_end_begin_{};

        T *elem_end_end_{};

        T *elem_end_last_{};

        [[nodiscard]] constexpr Block *block_ctrl_begin_() const noexcept {
            return details::to_address(block_ctrl_begin_fancy_);
        }

        constexpr void dealloc_block_(Block b) noexcept {
            if constexpr (typed_allocator_type::has_deallocate) {
                typed_allocator_type::deallocate(b);
            } else {
                typed_allocator_type::deallocate_n(b, details::block_elements_v<T>);
            }
        }

        constexpr Block alloc_block_() {
            return typed_allocator_type::allocate(details::block_elements_v<T>);
        }

        constexpr BlockFP alloc_ctrl_(::std::size_t const size) {
            return typed_allocator_pointer_type::allocate(size);
        }

        constexpr void dealloc_ctrl_() noexcept {
            if (block_ctrl_end_ != block_ctrl_begin_()) {
                if constexpr (typed_allocator_type::has_deallocate) {
                    typed_allocator_pointer_type::deallocate(block_ctrl_begin_fancy_);
                } else {
                    typed_allocator_pointer_type::deallocate_n(block_ctrl_begin_fancy_,
                                                               static_cast<::std::size_t>(
                                                                   block_ctrl_end_ - block_ctrl_begin_()));
                }
            }
        }

        constexpr void destroy_elems_() const noexcept
            requires ::std::is_trivially_destructible_v<T> && is_default_operation_ {
            /* */
        }


        constexpr void destroy_elems_() noexcept {
            auto const block_size = block_elem_size_();
            if (block_size) {
                for (auto const &i: ::std::ranges::subrange{elem_begin_begin_, elem_begin_end_}) {
                    ::std::destroy_at(::std::addressof(i));
                }
            }

            if (block_size > 2) {
                for (auto const block_begin: ::std::ranges::subrange{block_elem_begin_ + 1, block_elem_end_ - 1}) {
                    for (auto const &i:
                         ::std::ranges::subrange{block_begin, block_begin + details::block_elements_v<T>}) {
                        std::destroy_at(::std::addressof((i)));
                    }
                }
            }
            if (block_size > 1) {
                for (auto const &i: ::std::ranges::subrange{elem_end_begin_, elem_end_end_}) {
                    std::destroy_at(::std::addressof((i)));
                }
            }
        }


        constexpr void destroy_() noexcept {
            destroy_elems_();

            for (auto const i: ::std::ranges::subrange{block_alloc_begin_, block_alloc_end_}) {
                dealloc_block_(i);
            }
            dealloc_ctrl_();
        }

        template<typename U, typename V, typename W>
        constexpr void elem_begin_(U const begin, V const end, W const first) noexcept {
            elem_begin_begin_ = details::to_address(begin);
            elem_begin_end_ = details::to_address(end);
            elem_begin_first_ = details::to_address(first);
        }

        template<typename U, typename V, typename W>
        constexpr void elem_end_(U const begin, V const end, W const last) noexcept {
            elem_end_begin_ = details::to_address(begin);
            elem_end_end_ = details::to_address(end);
            elem_end_last_ = details::to_address(last);
        }

        [[nodiscard]] constexpr ::std::size_t block_elem_size_() const noexcept {
            return static_cast<::std::size_t>(block_elem_end_ - block_elem_begin_);
        }

        [[nodiscard]] constexpr ::std::size_t block_ctrl_size_() const noexcept {
            return static_cast<::std::size_t>(block_ctrl_end_ - block_ctrl_begin_());
        }

        [[nodiscard]] constexpr ::std::size_t block_alloc_size_() const noexcept {
            return static_cast<::std::size_t>(block_alloc_end_ - block_alloc_begin_);
        }

        constexpr void swap(deque &other) noexcept {
            using ::std::ranges::swap;
            swap(block_ctrl_begin_fancy_, other.block_ctrl_begin_fancy_);
            swap(block_ctrl_end_, other.block_ctrl_end_);
            swap(block_alloc_begin_, other.block_alloc_begin_);
            swap(block_alloc_end_, other.block_alloc_end_);
            swap(block_elem_begin_, other.block_elem_begin_);
            swap(block_elem_end_, other.block_elem_end_);
            swap(elem_begin_first_, other.elem_begin_first_);
            swap(elem_begin_begin_, other.elem_begin_begin_);
            swap(elem_begin_end_, other.elem_begin_end_);
            swap(elem_end_begin_, other.elem_end_begin_);
            swap(elem_end_end_, other.elem_end_end_);
            swap(elem_end_last_, other.elem_end_last_);
        }

    public:
        using value_type = T;
        using pointer = value_type *;
        using reference = value_type &;
        using const_pointer = pointer const;
        using const_reference = value_type const &;
        using size_type = ::std::size_t;
        using difference_type = ::std::ptrdiff_t;
        using iterator = details::deque_iterator<T, Block>;
        using reverse_iterator = ::std::reverse_iterator<details::deque_iterator<T, Block> >;
        using const_iterator = details::deque_iterator<T const, Block>;
        using const_reverse_iterator = ::std::reverse_iterator<details::deque_iterator<T const, Block> >;
        using allocator_type = allocator;

        constexpr deque() noexcept(::std::is_nothrow_default_constructible_v<allocator>)
            requires ::std::default_initializable<allocator>
        = default;

        explicit constexpr deque(allocator const & /*alloc*/) noexcept
            requires ::std::default_initializable<allocator> {
        }

        constexpr deque(deque const &other, allocator const & /*alloc*/) : deque(other) {
        }

        constexpr deque(deque &&other, allocator const & /*alloc*/) noexcept : deque(::std::move(other)) {
        }

        explicit constexpr deque(::std::size_t const count) {
            auto const [block_size, full_blocks, rem_elems] = details::calc_cap<T>(count);
            construct_guard_ guard(this);
            extent_block_(block_size);
            construct_(full_blocks, rem_elems);
            guard.release();
        }

        constexpr deque(::std::size_t const count, T const &t) {
            auto const [block_size, full_blocks, rem_elems] = details::calc_cap<T>(count);
            construct_guard_ guard(this);
            extent_block_(block_size);
            construct_(full_blocks, rem_elems, t);
            guard.release();
        }

        template<::std::input_iterator U, typename V>
        constexpr deque(U first, V last) {
            construct_guard_ guard(this);
            from_range_noguard_(::std::move(first), ::std::move(last));
            guard.release();
        }
#if (defined(__cpp_lib_from_range) && __cpp_lib_from_range >= 202207L) || __cplusplus > 202002L
        template<::std::ranges::input_range R>
            requires ::std::convertible_to<::std::ranges::range_value_t<R>, T>
        constexpr deque(::std::from_range_t, R &&rg) {
            construct_guard_ guard(this);
            from_range_noguard_(rg);
            guard.release();
        }
#endif


        constexpr deque(deque const &other) {
            if (!other.empty()) {
                construct_guard_ guard(this);
                from_range_noguard_(other.begin(), other.end());
                guard.release();
            }
        }

        constexpr deque(deque &&other) noexcept {
            other.swap(*this);
        }

        explicit constexpr deque(::std::initializer_list<T> const ilist) {
            if (ilist.size()) {
                construct_guard_ guard(this);
                from_range_noguard_(ilist.begin(), ilist.end());
                guard.release();
            }
        }


        constexpr ~deque() {
            destroy_();
        }

        constexpr bool empty() const noexcept {
            return elem_begin_begin_ == nullptr;
        }

        constexpr void clear() noexcept {
            destroy_elems_();
            block_elem_begin_ = block_alloc_begin_;
            block_elem_end_ = block_alloc_begin_;
            elem_begin_(nullptr, nullptr, nullptr);
            elem_end_(nullptr, nullptr, nullptr);
        }


        [[nodiscard]] constexpr ::std::size_t size() const noexcept {
            auto const block_size = block_elem_size_();
            ::std::size_t result = 0;
            if (block_size) {
                result += static_cast<unsigned long>(elem_begin_end_ - elem_begin_begin_);
            }
            if (block_size > 2) {
                result += (block_size - 2) * details::block_elements_v<T>;
            }
            if (block_size > 1) {
                result += static_cast<unsigned long>(elem_end_end_ - elem_end_begin_);
            }
            return result;
        }

        [[nodiscard]] static constexpr ::std::size_t max_size() noexcept {
            return static_cast<::std::size_t>(-1) / 2;
        }

#if !defined(NDEBUG)
        static_assert(::std::random_access_iterator<iterator>);
        static_assert(::std::sentinel_for<iterator, iterator>);
#endif

#if __has_cpp_attribute(__gnu__::__always_inline__)
        [[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
        [[msvc::forceinline]]
#endif
        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            if (block_elem_size_() == 0) {
                return const_iterator{nullptr, nullptr, nullptr, nullptr};
            }
            return const_iterator{block_elem_begin_, block_elem_end_, *block_elem_begin_, elem_begin_begin_};
        }
#if __has_cpp_attribute(__gnu__::__always_inline__)
        [[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
        [[msvc::forceinline]]
#endif
        [[nodiscard]] constexpr const_iterator end() const noexcept {
            if (block_elem_size_() == 0) {
                return const_iterator{nullptr, nullptr, nullptr, nullptr};
            }
            return const_iterator{block_elem_end_ - 1, block_elem_end_, *(block_elem_end_ - 1), elem_end_end_};
        }
#if __has_cpp_attribute(__gnu__::__always_inline__)
        [[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
        [[msvc::forceinline]]
#endif
        constexpr iterator begin() noexcept {
            return static_cast<const deque &>(*this).begin().remove_const_();
        }
#if __has_cpp_attribute(__gnu__::__always_inline__)
        [[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
        [[msvc::forceinline]]
#endif
        constexpr iterator end() noexcept {
            return static_cast<const deque &>(*this).end().remove_const_();
        }

        constexpr const_iterator cbegin() const noexcept {
            return begin();
        }

        [[nodiscard]] constexpr const_iterator cend() const noexcept {
            return end();
        }

        constexpr auto rbegin() noexcept {
            return reverse_iterator{end()};
        }

        constexpr auto rend() noexcept {
            return reverse_iterator{begin()};
        }

        constexpr auto rbegin() const noexcept {
            return const_reverse_iterator{end()};
        }

        constexpr auto rend() const noexcept {
            return const_reverse_iterator{begin()};
        }

        constexpr auto rcbegin() const noexcept {
            return const_reverse_iterator{end()};
        }

        constexpr auto rcend() const noexcept {
            return const_reverse_iterator{begin()};
        }

        [[nodiscard]] static constexpr allocator_type get_allocator() noexcept {
            return allocator_type{};
        }

        template<typename... V>
        constexpr T &emplace_back(V &&... v) {
            auto const block_size = block_elem_size_();
            if (elem_end_end_ != elem_end_last_) {
                return emplace_back_pre_(block_size, ::std::forward<V>(v)...);
            } else {
                reserve_one_back_();
                return emplace_back_post_(block_size, ::std::forward<V>(v)...);
            }
        }

        constexpr deque &operator=(const deque &other) {
            if (this != ::std::addressof(other)) {
                assign(other.begin(), other.end());
            }
            return *this;
        }

        constexpr deque &operator=(::std::initializer_list<T> ilist) {
            clear();
            if (ilist.size()) {
                auto const [block_size, full_blocks, rem_elems] = details::calc_cap<T>(ilist.size());
                extent_block_(block_size);
                construct_(full_blocks, rem_elems, ::std::ranges::begin(ilist), ::std::ranges::end(ilist));
            }
            return *this;
        }

        constexpr deque &operator=(deque &&other) noexcept {
            if (this == &other) {
                return *this;
            }
            other.swap(*this);
            return *this;
        }

        constexpr void assign_range(deque &&d) {
            *this = ::std::move(d);
        }

        constexpr void assign_range(deque const &d) {
            *this = d;
        }

        template<::std::ranges::input_range R>
            requires ::std::convertible_to<::std::ranges::range_value_t<R>, T>
        constexpr void assign_range(R &&rg) {
            clear();
            from_range_noguard_(::std::forward<R>(rg));
        }

        constexpr void assign(::std::size_t const count, T const &value) {
            clear();
            if (count) {
                auto const [block_size, full_blocks, rem_elems] = details::calc_cap<T>(count);
                extent_block_(block_size);
                construct_(full_blocks, rem_elems, value);
            }
            /*
            assign_range(::std::ranges::views::repeat(value, count));
            */
        }

        template<::std::input_iterator U, typename V>
        constexpr void assign(U first, V last) {
            clear();
            from_range_noguard_(::std::move(first), ::std::move(last));
        }

        constexpr void assign(::std::initializer_list<T> const ilist) {
            clear();
            from_range_noguard_(ilist.begin(), ilist.end());
        }

        template<typename... V>
        constexpr T &emplace_front(V &&... v) {
            auto const block_size = block_elem_size_();
            if (elem_begin_begin_ != elem_begin_first_) {
                return emplace_front_pre_(block_size, ::std::forward<V>(v)...);
            } else {
                reserve_one_front_();
                return emplace_front_post_(block_size, ::std::forward<V>(v)...);
            }
        }
#if __has_cpp_attribute(__gnu__::__always_inline__)
        [[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
        [[msvc::forceinline]]
#endif
        constexpr T &operator[](::std::size_t const pos) noexcept {
            auto const front_size = static_cast<::std::size_t>(elem_begin_begin_ - elem_begin_first_);
            auto const [block_step, elem_step] = details::calc_pos<T>(front_size, pos);
            auto const target_block = block_elem_begin_ + block_step;
            auto const check_block = target_block < block_elem_end_;
            auto const check_elem = (target_block + 1 == block_elem_end_)
                                        ? (::std::to_address(*target_block) + elem_step < elem_end_end_)
                                        : true;

            if (!(check_block && check_elem)) [[unlikely]] {
                fast_terminate();
            }

            return *((*target_block) + elem_step);
        }
#if __has_cpp_attribute(__gnu__::__always_inline__)
        [[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
        [[msvc::forceinline]]
#endif
        constexpr T const &operator[](::std::size_t const pos) const noexcept {
            auto const front_size = static_cast<::std::size_t>(elem_begin_begin_ - elem_begin_first_);
            auto const [block_step, elem_step] = details::calc_pos<T>(front_size, pos);
            auto const target_block = block_elem_begin_ + block_step;
            auto const check_block = target_block < block_elem_end_;
            auto const check_elem = (target_block + 1 == block_elem_end_)
                                        ? (::std::to_address(*target_block) + elem_step < elem_end_end_)
                                        : true;

            if (!(check_block && check_elem)) [[unlikely]] {
                fast_terminate();
            }

            return *((*target_block) + elem_step);
        }

        constexpr void shrink_to_fit() noexcept {
            if (block_alloc_size_() != 0) {
                for (auto const i: ::std::ranges::subrange{block_alloc_begin_, block_elem_begin_}) {
                    dealloc_block_(i);
                }
                block_alloc_begin_ = block_elem_begin_;
                for (auto const i: ::std::ranges::subrange{block_elem_end_, block_alloc_end_}) {
                    dealloc_block_(i);
                }
                block_alloc_end_ = block_elem_end_;
            }
        }

        constexpr void push_back(T const &t) {
            emplace_back(t);
        }

        constexpr void push_back(T &&t) {
            emplace_back(::std::move(t));
        }

        constexpr void push_front(T const &value) {
            emplace_front(value);
        }

        constexpr void push_front(T &&value) {
            emplace_front(::std::move(value));
        }


        constexpr void pop_back() noexcept {
            if (empty()) [[unlikely]] {
                fast_terminate();
            }
            --elem_end_end_;
            ::std::destroy_at(elem_end_begin_);
            if (elem_end_end_ == elem_end_begin_) {
                --block_elem_end_;
                auto const block_size = block_elem_size_();
                if (block_size == 1) {
                    elem_end_(elem_begin_begin_, elem_begin_end_, elem_begin_end_);
                } else if (block_size) {
                    auto const begin = *(block_elem_end_ - 1);
                    auto const last = begin + details::block_elements_v<T>;
                    elem_end_(begin, last, last);
                } else {
                    elem_begin_(nullptr, nullptr, nullptr);
                    elem_end_(nullptr, nullptr, nullptr);
                }
            } else if (block_elem_size_() == 1) {
                --elem_begin_end_;
            }
        }


        constexpr void pop_front() noexcept {
            if (empty()) [[unlikely]] {
                fast_terminate();
            }
            ::std::destroy_at(elem_begin_begin_);
            ++elem_begin_begin_;
            if (elem_begin_end_ == elem_begin_begin_) {
                ++block_elem_begin_;
                auto const block_size = block_elem_size_();

                if (block_size == 1) {
                    elem_begin_(elem_end_begin_, elem_end_end_, elem_end_begin_);
                } else if (block_size) {
                    auto const begin = *block_elem_begin_;
                    auto const last = begin + details::block_elements_v<T>;
                    elem_begin_(begin, last, begin);
                } else {
                    elem_begin_(nullptr, nullptr, nullptr);
                    elem_end_(nullptr, nullptr, nullptr);
                }
            } else if (block_elem_size_() == 1) {
                ++elem_end_begin_;
            }
        }

        constexpr T &front() noexcept {
            if (empty()) [[unlikely]] {
                fast_terminate();
            }
            return *(elem_begin_begin_);
        }

        constexpr T &back() noexcept {
            if (empty()) [[unlikely]] {
                fast_terminate();
            }
            return *(elem_end_end_ - 1);
        }

        constexpr T const &front() const noexcept {
            if (empty()) [[unlikely]] {
                fast_terminate();
            }
            return *(elem_begin_begin_);
        }

        constexpr T const &back() const noexcept {
            if (empty()) [[unlikely]] {
                fast_terminate();
            }
            return *(elem_end_end_ - 1);
        }

        template<::std::ranges::input_range R>
            requires ::std::convertible_to<::std::ranges::range_value_t<R>, T>
        constexpr void append_range(R &&rg) {
            partial_guard_<true> guard(this, size());
            append_range_noguard_(::std::forward<R>(rg));
            guard.release();
        }

        template<::std::ranges::input_range R>
            requires ::std::convertible_to<::std::ranges::range_value_t<R>, T>
        constexpr void prepend_range(R &&rg) {
            auto const old_size = size();
            partial_guard_<false> guard(this, old_size);
            prepend_range_noguard_(::std::forward<R>(rg));
            guard.release();
        }

        constexpr void resize(::std::size_t const new_size) {
            new_size == 0 ? clear() : resize_unified_(new_size);
        }

        constexpr void resize(::std::size_t const new_size, T const &t) {
            new_size == 0 ? clear() : resize_unified_(new_size, t);
        }

        template<typename... Args>
        constexpr iterator emplace(const_iterator const pos, Args &&... args) {
            auto const begin_pre = begin();
            auto const end_pre = end();
            if (pos == end_pre) {
                emplace_back(::std::forward<Args>(args)...);
                return end() - 1;
            }
            if (pos == begin_pre) {
                emplace_front(::std::forward<Args>(args)...);
                return begin();
            }

            auto const back_diff = static_cast<::std::size_t>(end_pre - pos);
            auto const front_diff = static_cast<::std::size_t>(pos - begin_pre);
            // NB:


            if (back_diff <= front_diff || (block_elem_size_() == 1 && elem_end_end_ != elem_end_last_)) {
                reserve_back_(2);
                emplace_back_noalloc_(::std::forward<Args>(args)...);

                auto new_pos = begin() + static_cast<::std::ptrdiff_t>(front_diff);
                back_emplace_(new_pos.block_elem_curr_, new_pos.elem_curr_);
                *new_pos = ::std::move(back());
                pop_back();
                return new_pos;
            } else {
                reserve_front_(2);
                emplace_front_noalloc_(::std::forward<Args>(args)...);
                auto new_pos = end() - static_cast<::std::ptrdiff_t>(back_diff);
                front_emplace_(new_pos.block_elem_curr_, new_pos.elem_curr_);
                *(--new_pos) = ::std::move(front());
                pop_front();
                return new_pos;
            }
        }

        constexpr iterator insert(const_iterator const pos, T const &value) {
            return emplace(pos, value);
        }

        constexpr iterator insert(const_iterator const pos, T &&value) {
            return emplace(pos, ::std::move(value));
        }

        template<::std::ranges::input_range R>
            requires ::std::convertible_to<::std::ranges::range_value_t<R>, T>
        constexpr iterator insert_range(const_iterator const pos, R &&rg) {
            if (::std::ranges::empty(rg)) {
                return pos.remove_const_();
            }
            auto const begin_pre = begin();
            auto const end_pre = end();
            if (pos == end_pre) {
                auto const old_size = size();
                append_range_noguard_(::std::forward<R>(rg));
                return begin() + old_size;
            }
            if (pos == begin_pre) {
                prepend_range_noguard_(::std::forward<R>(rg));
                return begin();
            }
            auto const back_diff = end_pre - pos;
            auto const front_diff = pos - begin_pre;

            if (back_diff <= front_diff) {
                auto const old_size = size();
                append_range_noguard_(::std::forward<R>(rg));
                ::std::ranges::rotate(begin() + front_diff, begin() + old_size, end());
                return begin() + front_diff;
            } else {
                auto const old_size = size();
                prepend_range_noguard_(::std::forward<R>(rg));
                auto const count = size() - old_size;
                ::std::ranges::rotate(begin(), begin() + count, begin() + (count + front_diff));
                return begin() + front_diff;
            }
        }


        template<::std::input_iterator U, typename V>
        constexpr iterator insert(const_iterator const pos, U first, V last) {
            if (first == last) {
                return pos.remove_const_();
            }
            auto const begin_pre = begin();
            auto const end_pre = end();
            if (pos == end_pre) {
                auto const old_size = size();
                append_range_noguard_(::std::forward<U>(first), ::std::forward<V>(last));
                return begin() + old_size;
            }
            if (pos == begin_pre) {
                prepend_range_noguard_(::std::forward<U>(first), ::std::forward<V>(last));
                return begin();
            }
            auto const back_diff = end_pre - pos;
            auto const front_diff = pos - begin_pre;
            if (back_diff <= front_diff) {
                auto const old_size = size();
                append_range_noguard_(::std::forward<U>(first), ::std::forward<V>(last));
                ::std::ranges::rotate(begin() + front_diff, begin() + old_size, end());
                return begin() + front_diff;
            } else {
                auto const old_size = size();
                prepend_range_noguard_(::std::forward<U>(first), ::std::forward<V>(last));
                auto const count = size() - old_size;
                ::std::ranges::rotate(begin(), begin() + count, begin() + (count + front_diff));
                return begin() + front_diff;
            }
        }

        constexpr iterator insert(const_iterator const pos, ::std::initializer_list<T> const ilist) {
            return insert(pos, ilist.begin(), ilist.end());
        }

        constexpr iterator insert(const_iterator const pos, ::std::size_t const count, T const &value) {
#if defined(__cpp_lib_ranges_repeat) && __cpp_lib_ranges_repeat >= 202207L && (!defined(__GNUC__) || __GNUC__ >= 15)
            return insert_range(pos, ::std::ranges::views::repeat(value, count));
#else
            auto iota_v = ::std::ranges::iota_view(static_cast<::std::size_t>(0), count);
            auto transform_func = [&](auto) -> T const & { return value; };
            auto repeat_view = ::std::ranges::transform_view(iota_v, transform_func);
            return insert_range(pos, repeat_view);
#endif
        }

        constexpr iterator erase(const_iterator const pos) {
            auto const begin_pre = begin();
            auto const end_pre = end();
            if (pos == begin_pre) {
                pop_front();
                return begin();
            }
            if (pos + 1 == end_pre) {
                pop_back();
                return end();
            }
            auto const back_diff = end_pre - pos;
            auto const front_diff = pos - begin_pre;
            if (back_diff <= front_diff) {
                ::std::ranges::move((pos + 1).remove_const_(), end(), pos.remove_const_());
                pop_back();
                return begin() + front_diff;
            } else {
                ::std::ranges::move_backward(begin(), pos.remove_const_(), (pos + 1).remove_const_());
                pop_front();
                return begin() + front_diff;
            }
        }

        constexpr iterator erase(const_iterator const first, const_iterator const last) {
            auto const begin_pre = begin();
            auto const end_pre = end();
            if (first == begin_pre) {
                pop_front_n_(last - first);
                return begin();
            }
            if (last == end_pre) {
                pop_back_n_(last - first);
                return end();
            }
            auto const back_diff = end_pre - last;
            auto const front_diff = first - begin_pre;
            if (back_diff <= front_diff) {
                ::std::ranges::move(last, end(), first.remove_const_());
                pop_back_n_(last - first);
                return begin() + front_diff;
            } else {
                ::std::ranges::move_backward(begin(), first.remove_const_(), last.remove_const_());
                pop_front_n_(last - first);
                return begin() + front_diff;
            }
        }

    private:
        struct ctrl_alloc_ {
            deque &d;
            BlockFP block_ctrl_begin_fancy{}; // A
            Block *block_ctrl_end{}; // D


            constexpr void replace_ctrl() const noexcept {
                d.block_ctrl_begin_fancy_ = block_ctrl_begin_fancy;
                d.block_ctrl_end_ = block_ctrl_end;
                d.block_alloc_begin_ = details::to_address(block_ctrl_begin_fancy);
                d.block_alloc_end_ = d.block_alloc_begin_;
                d.block_elem_begin_ = d.block_alloc_begin_;
                d.block_elem_end_ = d.block_alloc_begin_;
            }


            constexpr void replace_ctrl_back() const noexcept {
                d.align_elem_alloc_as_ctrl_back_(details::to_address(block_ctrl_begin_fancy));
                d.dealloc_ctrl_();


                d.block_ctrl_begin_fancy_ = block_ctrl_begin_fancy;
                d.block_ctrl_end_ = block_ctrl_end;
            }

            constexpr void replace_ctrl_front() const noexcept {
                d.align_elem_alloc_as_ctrl_front_(block_ctrl_end);
                d.dealloc_ctrl_();


                d.block_ctrl_begin_fancy_ = block_ctrl_begin_fancy;
                d.block_ctrl_end_ = block_ctrl_end;
            }


            constexpr ctrl_alloc_(deque &dq, ::std::size_t const ctrl_size) : d(dq) {
                auto const size = (ctrl_size + (4 - 1)) / 4 * 4;
                block_ctrl_begin_fancy = d.alloc_ctrl_(size);
                block_ctrl_end = details::to_address(block_ctrl_begin_fancy) + size;
            }
        };


        constexpr void align_alloc_as_ctrl_back_() noexcept {
            ::std::ranges::copy(block_alloc_begin_, block_alloc_end_, block_ctrl_begin_());
            auto const block_size = block_alloc_size_();
            block_alloc_begin_ = block_ctrl_begin_();
            block_alloc_end_ = block_ctrl_begin_() + block_size;
        }


        constexpr void align_alloc_as_ctrl_front_() noexcept {
            ::std::ranges::copy_backward(block_alloc_begin_, block_alloc_end_, block_ctrl_end_);
            auto const block_size = block_alloc_size_();
            block_alloc_end_ = block_ctrl_end_;
            block_alloc_begin_ = block_ctrl_end_ - block_size;
        }


        constexpr void align_elem_as_alloc_back_() noexcept {
            ::std::ranges::rotate(block_alloc_begin_, block_elem_begin_, block_elem_end_);
            auto const block_size = block_elem_size_();
            block_elem_begin_ = block_alloc_begin_;
            block_elem_end_ = block_alloc_begin_ + block_size;
        }


        constexpr void align_elem_as_alloc_front_() noexcept {
            ::std::ranges::rotate(block_elem_begin_, block_elem_end_, block_alloc_end_);
            auto const block_size = block_elem_size_();
            block_elem_end_ = block_alloc_end_;
            block_elem_begin_ = block_alloc_end_ - block_size;
        }


        constexpr void align_elem_alloc_as_ctrl_back_(Block *const ctrl_begin) noexcept {
            align_elem_as_alloc_back_();
            auto const alloc_block_size = block_alloc_size_();
            auto const elem_block_size = block_elem_size_();
            ::std::ranges::copy(block_alloc_begin_, block_alloc_end_, ctrl_begin);
            block_alloc_begin_ = ctrl_begin;
            block_alloc_end_ = ctrl_begin + alloc_block_size;
            block_elem_begin_ = ctrl_begin;
            block_elem_end_ = ctrl_begin + elem_block_size;
        }


        constexpr void align_elem_alloc_as_ctrl_front_(Block *const ctrl_end) noexcept {
            align_elem_as_alloc_front_();
            auto const alloc_block_size = block_alloc_size_();
            auto const elem_block_size = block_elem_size_();
            ::std::ranges::copy_backward(block_alloc_begin_, block_alloc_end_, ctrl_end);
            block_alloc_end_ = ctrl_end;
            block_alloc_begin_ = ctrl_end - alloc_block_size;
            block_elem_end_ = ctrl_end;
            block_elem_begin_ = ctrl_end - elem_block_size;
        }


        constexpr void extent_block_front_uncond_(::std::size_t const block_size) {
            if (!(block_alloc_begin_ != block_ctrl_begin_())) [[unlikely]] {
                fast_terminate();
            }
            if (!(block_alloc_begin_)) [[unlikely]] {
                fast_terminate();
            }
            for (::std::size_t i = 0; i != block_size; ++i) {
                --block_alloc_begin_;
                *block_alloc_begin_ = alloc_block_();
            }
        }


        constexpr void extent_block_back_uncond_(::std::size_t const block_size) {
            if (!(block_alloc_end_ != block_ctrl_end_)) [[unlikely]] {
                fast_terminate();
            }
            if (!(block_alloc_end_)) [[unlikely]] {
                fast_terminate();
            }
            for (::std::size_t i = 0; i != block_size; ++i) {
                *block_alloc_end_ = alloc_block_();
                ++block_alloc_end_;
            }
        }

        constexpr void reallocate_control_block_for_back_expansion_(::std::size_t add_block_size) {
            ctrl_alloc_ const ctrl{*this, block_alloc_size_() + add_block_size}; // may throw
            ctrl.replace_ctrl_back();

            extent_block_back_uncond_(add_block_size);
        }

        constexpr void reserve_back_(::std::size_t const add_elem_size) {
            auto const head_block_cap = static_cast<::std::size_t>(block_elem_begin_ - block_alloc_begin_) *
                                        details::block_elements_v<T>;

            auto const tail_block_cap = static_cast<::std::size_t>(block_alloc_end_ - block_elem_end_) *
                                        details::block_elements_v<T>;

            auto const tail_cap = static_cast<::std::size_t>(elem_end_last_ - elem_end_end_);

            auto const non_move_cap = tail_block_cap + tail_cap;

            if (non_move_cap >= add_elem_size) {
                return;
            }

            auto const move_cap = head_block_cap + non_move_cap;

            if (move_cap >= add_elem_size) {
                align_elem_as_alloc_back_();
                return;
            }

            auto const add_block_size =
                    (add_elem_size - move_cap + details::block_elements_v<T> - 1) / details::block_elements_v<T>;

            auto const ctrl_cap = static_cast<::std::size_t>(
                                      (block_alloc_begin_ - block_ctrl_begin_()) + (block_ctrl_end_ - block_alloc_end_))
                                  *
                                  details::block_elements_v<T> +
                                  move_cap;

            if (ctrl_cap >= add_elem_size) {
                align_elem_alloc_as_ctrl_back_(block_ctrl_begin_());
                extent_block_back_uncond_(add_block_size);
            } else [[unlikely]] {
                reallocate_control_block_for_back_expansion_(add_block_size);
            }
            extent_block_back_uncond_(add_block_size);
        }


        constexpr void reserve_one_back_() {
            if (block_alloc_end_ != block_elem_end_) {
                return;
            }
            if (block_elem_begin_ != block_alloc_begin_) {
                align_elem_as_alloc_back_();
                return;
            }
            if ((block_alloc_begin_ - block_ctrl_begin_()) + (block_ctrl_end_ - block_alloc_end_) != 0) {
                align_elem_alloc_as_ctrl_back_(block_ctrl_begin_());
            } else {
                ctrl_alloc_ const ctrl{*this, block_alloc_size_() + 1}; // may throw
                ctrl.replace_ctrl_back();
            }
            extent_block_back_uncond_(1);
        }


        constexpr void reserve_front_(::std::size_t const add_elem_size) {
            auto const head_block_alloc_cap = static_cast<::std::size_t>(block_elem_begin_ - block_alloc_begin_) *
                                              details::block_elements_v<T>;

            auto const tail_block_alloc_cap = static_cast<::std::size_t>(block_alloc_end_ - block_elem_end_) *
                                              details::block_elements_v<T>;

            auto const head_cap = static_cast<::std::size_t>(elem_begin_begin_ - elem_begin_first_);

            auto const non_move_cap = head_block_alloc_cap + head_cap;

            if (non_move_cap >= add_elem_size) {
                return;
            }

            auto const move_cap = non_move_cap + tail_block_alloc_cap;

            if (move_cap >= add_elem_size) {
                align_elem_as_alloc_front_();
                return;
            }

            auto const add_block_size =
                    (add_elem_size - move_cap + details::block_elements_v<T> - 1) / details::block_elements_v<T>;

            auto const ctrl_cap = static_cast<::std::size_t>(
                                      (block_alloc_begin_ - block_ctrl_begin_()) + (block_ctrl_end_ - block_alloc_end_))
                                  *
                                  details::block_elements_v<T> +
                                  move_cap;
            if (ctrl_cap >= add_elem_size) {
                align_elem_alloc_as_ctrl_front_(block_ctrl_end_);
            } else {
                ctrl_alloc_ const ctrl{*this, block_alloc_size_() + add_block_size}; // may throw
                ctrl.replace_ctrl_front();
            }

            extent_block_front_uncond_(add_block_size);
        }


        constexpr void reserve_one_front_() {
            if (block_elem_begin_ != block_alloc_begin_) {
                return;
            }
            if (block_alloc_end_ != block_elem_end_) {
                align_elem_as_alloc_front_();
                return;
            }
            if ((block_alloc_begin_ - block_ctrl_begin_()) + (block_ctrl_end_ - block_alloc_end_) != 0) {
                align_elem_alloc_as_ctrl_front_(block_ctrl_end_);
            } else {
                ctrl_alloc_ const ctrl{*this, block_alloc_size_() + 1}; // may throw
                ctrl.replace_ctrl_front();
            }
            extent_block_front_uncond_(1);
        }

        class construct_guard_ {
            deque *d;

        public:
            constexpr explicit construct_guard_(deque *dp) noexcept : d(dp) {
            }

            constexpr void release() noexcept {
                d = nullptr;
            }

            constexpr ~construct_guard_() {
                if (d) {
                    d->destroy_();
                }
            }
        };


        constexpr void extent_block_(::std::size_t const new_block_size) {
            if (new_block_size != 0) {
                auto const ctrl_block_size = block_ctrl_size_();
                auto const alloc_block_size = block_alloc_size_();
                if (ctrl_block_size == 0) {
                    ctrl_alloc_ const ctrl(*this, new_block_size); // may throw
                    ctrl.replace_ctrl();
                    extent_block_back_uncond_(new_block_size); // may throw
                    return;
                }
                if (alloc_block_size >= new_block_size) {
                    return;
                }
                if (ctrl_block_size < new_block_size) {
                    ctrl_alloc_ const ctrl(*this, new_block_size); // may throw
                    ctrl.replace_ctrl_back();
                } else {
                    align_alloc_as_ctrl_back_();
                }
                extent_block_back_uncond_(new_block_size - alloc_block_size); // may throw
            }
        }

        template<typename... Ts>
        constexpr void construct_(::std::size_t const full_blocks, ::std::size_t const rem_elems, Ts &&... ts) {
            if (full_blocks) {
                auto const begin = *block_elem_end_;
                auto const end = begin + details::block_elements_v<T>;
                if constexpr (sizeof...(Ts) == 0) {
                    if constexpr (is_default_operation_) {
                        ::std::ranges::uninitialized_value_construct(begin, end);
                        elem_begin_(begin, end, begin);
                    } else {
                        elem_begin_(begin, begin, begin);
                        for (auto &i: ::std::ranges::subrange(begin, end)) {
                            ::std::construct_at(&i);
                            ++elem_begin_end_;
                        }
                    }
                } else if constexpr (sizeof...(Ts) == 1) {
                    if constexpr (is_default_operation_) {
                        ::std::ranges::uninitialized_fill(begin, end, ts...);
                        elem_begin_(begin, end, begin);
                    } else {
                        elem_begin_(begin, begin, begin);
                        for (auto &i: ::std::ranges::subrange(begin, end)) {
                            ::std::construct_at(&i, ts...);
                            ++elem_begin_end_;
                        }
                    }
                } else if constexpr (sizeof...(Ts) == 2) {
#if defined(__cpp_pack_indexing)
                    auto [src_begin, src_end] = details::get_first_two(ts...);
#else
                    auto [src_begin, src_end] = details::get_first_two(::std::forward_as_tuple(ts...));
#endif
                    ::std::ranges::uninitialized_copy(src_begin, ::std::unreachable_sentinel, begin,
                                                      begin + details::block_elements_v<T>);
                    src_begin += details::block_elements_v<T>;
                    elem_begin_(begin, end, begin);
                } else {
                    static_assert(false);
                }
                elem_end_(begin, end, end);
                ++block_elem_end_;
            }
            if (full_blocks > 1) {
                for (::std::size_t i{0};
                     i != full_blocks - 1; ++i) {
                    auto const begin = *block_elem_end_;
                    auto const end = begin + details::block_elements_v<T>;
                    if constexpr (sizeof...(Ts) == 0) {
                        if constexpr (is_default_operation_) {
                            ::std::ranges::uninitialized_value_construct(begin, end);
                            elem_end_(begin, end, elem_end_last_);
                        } else {
                            elem_end_(begin, begin, elem_end_last_);
                            for (auto &j: ::std::ranges::subrange(begin, end)) {
                                ::std::construct_at(&j);
                                ++elem_end_end_;
                            }
                        }
                    } else if constexpr (sizeof...(Ts) == 1) {
                        if constexpr (is_default_operation_) {
                            ::std::ranges::uninitialized_fill(begin, end, ts...);
                            elem_end_(begin, end, elem_end_last_);
                        } else {
                            elem_end_(begin, begin, elem_end_last_);
                            for (auto &j: ::std::ranges::subrange(begin, end)) {
                                ::std::construct_at(&j, ts...);
                                ++elem_end_end_;
                            }
                        }
                    } else if constexpr (sizeof...(Ts) == 2) {
#if defined(__cpp_pack_indexing)
                        auto [src_begin, src_end] = details::get_first_two(ts...);
#else
                        auto [src_begin, src_end] = details::get_first_two(::std::forward_as_tuple(ts...));
#endif
                        ::std::ranges::uninitialized_copy(src_begin, ::std::unreachable_sentinel, begin,
                                                          begin + details::block_elements_v<T>);
                        src_begin += details::block_elements_v<T>;
                        elem_end_(begin, end, elem_end_last_);
                    } else {
                        static_assert(false);
                    }
                    ++block_elem_end_;
                }
                elem_end_last_ = elem_end_end_;
            }
            if (rem_elems) {
                auto const begin = *block_elem_end_;
                auto const end = begin + rem_elems;
                if constexpr (sizeof...(Ts) == 0) {
                    if constexpr (is_default_operation_) {
                        ::std::ranges::uninitialized_value_construct(begin, end);
                        elem_end_(begin, end, begin + details::block_elements_v<T>);
                    } else {
                        elem_end_(begin, begin, begin + details::block_elements_v<T>);
                        for (auto &i: ::std::ranges::subrange(begin, end)) {
                            ::std::construct_at(&i);
                            ++elem_end_end_;
                        }
                    }
                } else if constexpr (sizeof...(Ts) == 1) {
                    if constexpr (is_default_operation_) {
                        ::std::ranges::uninitialized_fill(begin, end, ts...);
                        elem_end_(begin, end, begin + details::block_elements_v<T>);
                    } else {
                        elem_end_(begin, begin, begin + details::block_elements_v<T>);
                        for (auto &i: ::std::ranges::subrange(begin, end)) {
                            ::std::construct_at(&i, ts...);
                            ++elem_end_end_;
                        }
                    }
                } else if constexpr (sizeof...(Ts) == 2) {
#if defined(__cpp_pack_indexing)
                    auto [src_begin, src_end] = details::get_first_two(ts...);
#else
                    auto [src_begin, src_end] = details::get_first_two(::std::forward_as_tuple(ts...));
#endif
                    ::std::ranges::uninitialized_copy(src_begin, src_end, begin, ::std::unreachable_sentinel);
                    elem_end_(begin, end, begin + details::block_elements_v<T>);
                } else {
                    static_assert(false);
                }
                if (full_blocks == 0) {
                    elem_begin_(begin, end, begin);
                }
                ++block_elem_end_;
            }
        }

        template<typename... V>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
        constexpr T &emplace_back_pre_(::std::size_t const block_size, V &&... v) {
            auto const end = elem_end_end_;
            std::construct_at(end, ::std::forward<V>(v)...);
            elem_end_end_ = end + 1;

            if (block_size == 1) {
                elem_begin_end_ = end + 1;
            }
            return *end;
        }


        template<typename... V>
        constexpr T &emplace_back_post_(::std::size_t const block_size, V &&... v) {
            auto const begin = ::std::to_address(*block_elem_end_);
            ::std::construct_at(begin, ::std::forward<V>(v)...);
            elem_end_(begin, begin + 1, begin + details::block_elements_v<T>);
            ++block_elem_end_;

            if (block_size == 0) {
                elem_begin_(begin, begin + 1, begin);
            }
            return *begin;
        }

        template<::std::input_iterator U, typename V>
        constexpr void from_range_noguard_(U &&first, V &&last) {
            for (; first != last; ++first) {
                emplace_back(*first);
            }
        }

        template<::std::random_access_iterator U>
        constexpr void from_range_noguard_(U &&first, U &&last) {
            if (first != last) {
                auto const [block_size, full_blocks, rem_elems] = details::calc_cap<T>(
                    static_cast<::std::size_t>(last - first));
                extent_block_(block_size);
                construct_(full_blocks, rem_elems, ::std::forward<U>(first), ::std::forward<U>(last));
            }
        }

        template<typename R>
        constexpr void from_range_noguard_(R &&rg) {
            if constexpr (requires { is_iterator_(::std::ranges::begin(rg)); }) {
                from_range_noguard_(::std::ranges::begin(rg), ::std::ranges::end(rg));
            } else if constexpr (::std::ranges::sized_range<R>) {
                if (auto const size = ::std::ranges::size(rg)) {
                    auto const [block_size, full_blocks, rem_elems] = details::calc_cap<T>(size);
                    extent_block_(block_size);
                    construct_(full_blocks, rem_elems, ::std::ranges::begin(rg), ::std::ranges::end(rg));
                }
            } else if constexpr (::std::random_access_iterator<decltype(::std::ranges::begin(rg))>) {
                from_range_noguard_(::std::ranges::begin(rg), ::std::ranges::end(rg));
            }
#if defined(__cpp_lib_ranges_reserve_hint) && __cpp_lib_ranges_reserve_hint >= 202502L
        else if constexpr (::std::ranges::approximately_sized_range<R>)
        {
            if (auto const size = ::std::ranges::reserve_hint(rg))
            {
                reserve_back(size);
                auto begin = ::std::ranges::begin(rg);
                auto end = ::std::ranges::end(rg);
                for (; begin != end; ++begin)
                {
                    emplace_back_noalloc_(*begin);
                }
            }
        }
#endif
            else {
                from_range_noguard_(::std::ranges::begin(rg), ::std::ranges::end(rg));
            }
        }

        template<typename... V>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
        constexpr T &emplace_front_pre_(::std::size_t const block_size, V &&... v) {
            auto const begin = ::std::to_address(elem_begin_begin_ - 1);
            ::std::construct_at(begin, ::std::forward<V>(v)...); //may throw
            elem_begin_begin_ = begin;
            if (block_size == 1) {
#if __has_cpp_attribute(assume) && __cplusplus >= 202302L
            [[assume(begin + 1 == elem_begin_begin_)]];
#endif
                elem_end_begin_ = begin;
            }
            return *begin;
        }


        template<typename... V>
        constexpr T &emplace_front_post_(::std::size_t const block_size, V &&... v) {
            auto const block = block_elem_begin_ - 1;
            auto const first = ::std::to_address(*block);
            auto const end = first + details::block_elements_v<T>;
            ::std::construct_at(end - 1, ::std::forward<V>(v)...);
            elem_begin_(end - 1, end, first);
#if __has_cpp_attribute(assume) && __cplusplus >= 202302L
        [[assume(block + 1 == block_elem_begin_)]];
#endif
            --block_elem_begin_;

            if (block_size == 0) {
                elem_end_(end - 1, end, end);
            }
            return *(end - 1);
        }

        constexpr void pop_back_n_(::std::size_t const count) noexcept {
            for (auto i = 0; i != count; ++i) {
                if (empty()) [[unlikely]] {
                    fast_terminate();
                }
                pop_back();
            }
        }

        constexpr void pop_front_n_(::std::size_t const count) noexcept {
            for (::std::size_t i = 0; i != count; ++i) {
                if (empty()) [[unlikely]] {
                    fast_terminate();
                }
                pop_front();
            }
        }

        template<bool back>
        class partial_guard_ {
            deque *d;
            ::std::size_t const size;

        public:
            constexpr partial_guard_(deque *dp, ::std::size_t const old_size) noexcept : d(dp), size(old_size) {
            }

            constexpr void release() noexcept {
                d = nullptr;
            }

            constexpr ~partial_guard_() {
                if (d != nullptr) {
                    if constexpr (back) {
                        d->resize_shrink_(d->size(), size);
                    } else {
                        d->pop_front_n_(d->size() - size);
                    }
                }
            }
        };


        template<typename... V>
        constexpr T &emplace_front_noalloc_(V &&... v) {
            auto const block_size = block_elem_size_();
            if (elem_begin_begin_ != elem_begin_first_) {
                return emplace_front_pre_(block_size, ::std::forward<V>(v)...);
            } else {
                return emplace_front_post_(block_size, ::std::forward<V>(v)...);
            }
        }


        template<typename... V>
        constexpr T &emplace_back_noalloc_(V &&... v) {
            auto const block_size = block_elem_size_();
            if (elem_end_end_ != elem_end_last_) {
                return emplace_back_pre_(block_size, ::std::forward<V>(v)...);
            } else {
                return emplace_back_post_(block_size, ::std::forward<V>(v)...);
            }
        }

        template<::std::input_iterator U, typename V>
        constexpr void append_range_noguard_(U &&first, V &&last) {
            for (; first != last; ++first) {
                emplace_back(*first);
            }
        }

        template<::std::random_access_iterator U>
        constexpr void append_range_noguard_(U &&first, U &&last) {
            reserve_back_(static_cast<::std::size_t>(last - first));
            for (; first != last; ++first) {
                emplace_back_noalloc_(*first);
            }
        }

        template<typename R>
        constexpr void append_range_noguard_(R &&rg) {
            if (::std::ranges::empty(rg)) {
                return;
            }
#if defined(__cpp_lib_ranges_reserve_hint) && __cpp_lib_ranges_reserve_hint >= 202502L
        if constexpr (::std::ranges::approximately_sized_range<R>)
        {
            reserve_back_(::std::ranges::reserve_hint(rg));
#else
            if constexpr (::std::ranges::sized_range<R>) {
                reserve_back_(::std::ranges::size(rg));
#endif
                for (auto &&i: rg) {
                    emplace_back_noalloc_(::std::forward<decltype(i)>(i));
                }
            } else {
                append_range_noguard_(::std::ranges::begin(rg), ::std::ranges::end(rg));
            }
        }

        template<::std::input_iterator U, typename V>
        constexpr void prepend_range_noguard_(U &&first, V &&last) {
            auto const old_size = size();
            for (; first != last; ++first) {
                emplace_front(*first);
            }
            ::std::ranges::reverse(begin(), begin() + (size() - old_size));
        }

        template<::std::bidirectional_iterator U>
        constexpr void prepend_range_noguard_(U &&first, U &&last) {
            for (; first != last;) {
                --last;
                emplace_front(*last);
            }
        }

        template<::std::random_access_iterator U>
        constexpr void prepend_range_noguard_(U &&first, U &&last) {
            reserve_front_(static_cast<::std::size_t>(last - first));
            for (; first != last;) {
                --last;
                emplace_front_noalloc_(*last);
            }
        }

        template<typename R>
        constexpr void prepend_range_noguard_(R &&rg) {
            if (::std::ranges::empty(rg)) {
                return;
            }
#if defined(__cpp_lib_ranges_reserve_hint) && __cpp_lib_ranges_reserve_hint >= 202502L
        if constexpr (::std::ranges::approximately_sized_range<R> && ::std::ranges::bidirectional_range<R>)
        {
            reserve_front_(::std::ranges::reserve_hint(rg));
#else
            if constexpr (::std::ranges::sized_range<R> && ::std::ranges::bidirectional_range<R>) {
                reserve_front_(::std::ranges::size(rg));
#endif
                auto first = ::std::ranges::begin(rg);
                auto last = ::std::ranges::end(rg);
                for (; first != last;) {
                    --last;
                    emplace_front_noalloc_(*last);
                }
            } else if constexpr (::std::ranges::bidirectional_range<R>) {
                prepend_range_noguard_(::std::ranges::begin(rg), ::std::ranges::end(rg));
            }
#if defined(__cpp_lib_ranges_reserve_hint) && __cpp_lib_ranges_reserve_hint >= 202502L
        else if constexpr (::std::ranges::approximately_sized_range<R>)
        {
            reserve_front(::std::ranges::reserve_hint(rg));
            auto const old_size = size();
            for (auto &&i : rg)
            {
                emplace_front_noalloc_(::std::forward<decltype(i)>(i));
            }
            ::std::ranges::reverse(begin(), begin() + (size() - old_size));
        }
#endif
            else if constexpr (::std::ranges::sized_range<R>) {
                auto const count = ::std::ranges::size(rg);
                reserve_front_(count);
                for (auto &&i: rg) {
                    emplace_front_noalloc_(::std::forward<decltype(i)>(i));
                }
                ::std::ranges::reverse(begin(), begin() + count);
            } else {
                prepend_range_noguard_(::std::ranges::begin(rg), ::std::ranges::end(rg));
            }
        }

        constexpr void resize_shrink_(::std::size_t const old_size, ::std::size_t const new_size) noexcept {
            if (old_size < new_size) [[unlikely]] {
                fast_terminate();
            }
            if constexpr (::std::is_trivially_destructible_v<T> && is_default_operation_) {
                auto const [block_step, elem_step] =
                        details::calc_pos<T>(static_cast<::std::size_t>(elem_begin_begin_ - elem_begin_first_),
                                             new_size);
                if (block_step == 0) {
                    auto const begin = elem_begin_first_;
                    elem_end_(elem_begin_begin_, begin + elem_step, begin + details::block_elements_v<T>);
                    block_elem_end_ = block_elem_begin_ + 1;
                    elem_begin_(elem_begin_begin_, begin + elem_step, begin);
                } else {
                    auto const target_block = block_elem_begin_ + block_step;
                    auto const begin = *target_block;
                    elem_end_(begin, begin + elem_step, begin + details::block_elements_v<T>);
                    block_elem_end_ = target_block + 1;
                }
            } else {
                auto const count = old_size - new_size;
                for (::std::size_t i = 0; i != count; ++i) {
                    if (empty()) [[unlikely]] {
                        fast_terminate();
                    }
                    pop_back();
                }
            }
        }

        template<typename... Ts>
        constexpr void resize_unified_(::std::size_t const new_size, Ts &&... ts) {
            if (auto const old_size = size(); new_size < old_size) {
                resize_shrink_(old_size, new_size);
            } else {
                auto const diff = new_size - old_size;
                reserve_back_(diff);
                partial_guard_<true> guard(this, old_size);
                for (::std::size_t i = 0; i != diff; ++i) {
                    emplace_back_noalloc_(ts...);
                }
                guard.release();
            }
        }

        constexpr void back_emplace_(Block *const block_curr, T *const elem_curr) {
            auto const block_end = block_elem_end_;
            auto const block_size = static_cast<::std::size_t>(block_end - block_curr) - 1;
            auto last_elem = elem_end_begin_;
            auto end = elem_end_end_;
            emplace_back_noalloc_(::std::move(back()));
            if (block_size > 0) {
                auto const begin = last_elem;
                ::std::ranges::move_backward(begin, end - 1, end);
            }
            if (block_size > 1) {
                auto target_block_end = block_end - 1;
                for (; target_block_end != block_curr + 1;) {
                    --target_block_end;
                    auto const target_begin = ::std::to_address(*target_block_end);
                    auto const target_end = target_begin + details::block_elements_v<T>;
                    *last_elem = ::std::move(*(target_end - 1));
                    last_elem = target_begin;
                    ::std::ranges::move_backward(target_begin, target_end - 1, target_end);
                }
            } {
                if (block_end - 1 != block_curr) {
                    end = ::std::to_address(*block_curr + details::block_elements_v<T>);
                    *last_elem = ::std::move(*(end - 1));
                }
                ::std::ranges::move_backward(elem_curr, end - 1, end);
            }
        }

        constexpr void front_emplace_(Block *const block_curr, T *const elem_curr) {
            auto const block_begin = block_elem_begin_;
            auto const block_size = static_cast<::std::size_t>(block_curr - block_begin);
            auto const last_elem_begin = elem_begin_begin_;
            auto last_elem_end = elem_begin_end_;
            emplace_front_noalloc_(::std::move(front()));

            if (block_begin == block_curr) {
                last_elem_end = elem_curr;
            }

            ::std::ranges::move(last_elem_begin + 1, last_elem_end, last_elem_begin);
            if (block_size > 1) {
                auto target_block_begin = block_begin + 1;
                for (; target_block_begin != block_curr; ++target_block_begin) {
                    auto const begin = ::std::to_address(*target_block_begin);
                    auto const end = begin + details::block_elements_v<T>;
                    *(last_elem_end - 1) = ::std::move(*begin);
                    last_elem_end = end;
                    ::std::ranges::move(begin + 1, end, begin);
                }
            }
            if (block_size > 0) {
                auto const begin = ::std::to_address(*block_curr);
                if (elem_curr != begin) {
                    *(last_elem_end - 1) = ::std::move(*begin);
                    ::std::ranges::move(begin + 1, elem_curr, begin);
                }
            }
        }

        static inline constexpr auto synth_three_way_ = []<class U, class V>(const U &u, const V &v) {
            if constexpr (::std::three_way_comparable_with<U, V>) {
                return u <=> v;
            } else {
                if (u < v)
                    return ::std::weak_ordering::less;
                if (v < u)
                    return ::std::weak_ordering::greater;
                return ::std::weak_ordering::equivalent;
            }
        };
    };

    template<::std::movable T, typename allocator1, typename allocator2>
        requires ::std::equality_comparable<T>
    inline constexpr bool operator==(deque<T, allocator1> const &lhs, deque<T, allocator2> const &rhs) noexcept {
        if (auto const s = lhs.size(); s != rhs.size())
            return false;
        else if (s != 0) {
            auto first = lhs.begin();
            auto last = lhs.end();
            auto first1 = rhs.begin();
            for (; first != last; ++first, ++first1) {
                if (*first != *first1)
                    return false;
            }
        }
        return true;
    }

#if defined(__cpp_lib_three_way_comparison)
    template<::std::movable T, typename allocator1, typename allocator2>
        requires ::std::three_way_comparable<T>
    inline constexpr auto operator<=>(deque<T, allocator1> const &lhs, deque<T, allocator2> const &rhs) noexcept
        requires requires(T const &t, T const &t1)
        {
            { t < t1 } -> ::std::convertible_to<bool>;
        } {
        return ::std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
                                                        []<class U, class V>(const U &u, const V &v) {
                                                            if constexpr (::std::three_way_comparable_with<U, V>)
                                                                return u <=> v;
                                                            else {
                                                                if (u < v)
                                                                    return ::std::weak_ordering::less;
                                                                if (v < u)
                                                                    return ::std::weak_ordering::greater;
                                                                return ::std::weak_ordering::equivalent;
                                                            }
                                                        });
    }
#endif

    template<::std::movable T, typename allocator>
    constexpr void swap(deque<T, allocator> &lhs, deque<T, allocator> &rhs) noexcept {
        lhs.swap(rhs);
    }
}
