/*!
 *  Copyright (c) 2016 by Contributors
 * \file tensor.h
 * \brief Dataflow tensor object
 */
#ifndef TVM_TENSOR_H_
#define TVM_TENSOR_H_

#include <tvm/container.h>
#include <ir/FunctionBase.h>
#include <string>
#include <vector>
#include <type_traits>

#include "./base.h"
#include "./expr.h"

namespace tvm {

// Internal node container of Tensor
class TensorNode;
// internal node container for Operation
class OperationNode;

using Halide::IR::FunctionRef;

/*!
 * \brief Tensor structure representing a possible input,
 *  or intermediate computation result.
 */
class Tensor : public FunctionRef {
 public:
  /*! \brief default constructor, used internally */
  Tensor() {}
  explicit Tensor(std::shared_ptr<Node> n) : FunctionRef(n) {}
  /*!
   * \brief constructor of input tensor
   * \param shape Shape of the tensor.
   * \param name optional name of the Tensor.
   * \param dtype The data type of the input tensor.
   */
  explicit Tensor(Array<Expr> shape,
                  std::string name = "tensor",
                  Type dtype = Float(32));
  /*!
   * \brief access the internal node container
   * \return the pointer to the internal node container
   */
  inline const TensorNode* operator->() const;
  /*! \return The dimension of the tensor */
  inline size_t ndim() const;
  /*!
   * \brief Take elements from the tensor
   * \param args The indices
   * \return the result expression representing tensor read.
   */
  template<typename... Args>
  inline Expr operator()(Args&& ...args) const {
    Array<Expr> indices{std::forward<Args>(args)...};
    return operator()(indices);
  }
  /*!
   * \brief Take elements from the tensor
   * \param indices the indices.
   * \return the result expression representing tensor read.
   */
  Expr operator()(Array<Expr> indices) const;
  /*!
   * \brief data structure to represent a slice that fixes first k coordinates.
   *  This is used to enable syntax sugar of Tensor[x][y][z] to get the element.
   */
  class Slice {
   public:
    // construct via tensor and indices
    Slice(const Tensor& tensor, std::vector<Expr> indices)
        : tensor_(tensor), indices_(indices) {}
    /*!
     * \brief get i-th slice from the current slice.
     * \param i the index of the coordinate
     * \return the subsequent slice.
     */
    inline Slice operator[](Expr i) {
      std::vector<Expr> other = indices_;
      other.emplace_back(i);
      return Slice(tensor_, other);
    }
    /*!
     * \brief Convert slice to expression.
     *  This is only valid when all the coordinates are fully specified.
     * \return the corresponding expression of this slice.
     */
    inline operator Expr() const {
      return tensor_(indices_);
    }

   private:
    const Tensor& tensor_;
    std::vector<Expr> indices_;
  };
  /*!
   * \brief get i-th slice from the current Tensor.
   * \param i the index of the coordinate
   * \return the subsequent slice.
   */
  inline Slice operator[](Expr i) const {
    return Slice(*this, {i});
  }
  /*! \brief specify container node */
  using ContainerType = TensorNode;
};

/*! \brief Operation that produces tensors */
class Operation : public NodeRef {
 public:
  /*! \brief default constructor  */
  Operation() {}
  explicit Operation(std::shared_ptr<Node> n) : NodeRef(n) {}
  /*!
   * \brief access the internal node container
   * \return the pointer to the internal node container
   */
  inline const OperationNode* operator->() const;
  /*!
   * \brief get the i-th output of the operation.
   * \param i the output index.
   * \return The i-th output.
   */
  Tensor output(size_t i) const;
  /*! \brief specify container node */
  using ContainerType = OperationNode;
};

/*! \brief Node to represent a tensor */
class TensorNode : public FunctionBaseNode {
 public:
  /*! \brief The shape of the tensor */
  Array<Expr> shape;
  /*! \brief optional name of the tensor */
  std::string name;
  /*! \brief data type in the content of the tensor */
  Type dtype;
  /*! \brief the source operation, can be None */
  Operation op;
  /*! \brief the output index from source operation */
  int value_index{0};
  /*! \brief constructor */
  TensorNode() {}

  void VisitAttrs(AttrVisitor* v) final {
    v->Visit("shape", &shape);
    v->Visit("name", &name);
    v->Visit("dtype", &dtype);
    v->Visit("op", &op);
    v->Visit("value_index", &value_index);
  }
  const std::string& func_name() const final {
    return name;
  }
  int outputs() const final {
    return 1;
  }
  static Tensor make(Array<Expr> shape,
                     std::string name,
                     Type dtype,
                     Operation op,
                     int value_index);

  static constexpr const char* _type_key = "Tensor";
  TVM_DECLARE_NODE_TYPE_INFO(TensorNode);
};

/*!
 * \brief base class of operation node.
 */
class OperationNode : public Node {
 public:
  /*! \brief optional name of the operation */
  std::string name;
  /*! \return the list of iteration variable at root */
  virtual Array<IterVar> root_iter_vars() const = 0;
  /*! \return number of outputs of this op */
  virtual size_t num_outputs() const = 0;
  /*! \return name of i-th output */
  virtual std::string output_name(size_t i) const = 0;
  /*! \return type of i-th output */
  virtual Type output_dtype(size_t i) const = 0;
  /*! \return shape of i-th output */
  virtual Array<Expr> output_shape(size_t i) const = 0;
};

// Implementations of inline functions
inline const OperationNode* Operation::operator->() const {
  return static_cast<const OperationNode*>(node_.get());
}

inline const TensorNode* Tensor::operator->() const {
  return static_cast<const TensorNode*>(node_.get());
}

inline size_t Tensor::ndim() const {
  return (*this)->shape.size();
}

// macro to turn every operation of slice to expression
#define DEFINE_OVERLOAD_SLICE_UNARY_OP(Op)                              \
  inline Expr operator Op (const Tensor::Slice& a) {                    \
    return Op a.operator Expr() ;                                       \
  }

#define DEFINE_OVERLOAD_SLICE_BINARY_OP(Op)                             \
  template<typename T>                                                  \
  inline Expr operator Op (const Tensor::Slice& a, const T& b) {        \
    return a.operator Expr() Op b;                                      \
  }                                                                     \
  template<typename T>                                                  \
  inline Expr operator Op (const T& a, const Tensor::Slice& b) {        \
    return a Op b.operator Expr();                                      \
  }                                                                     \
  inline Expr operator Op (const Tensor::Slice& a, const Tensor::Slice& b) { \
    return a.operator Expr() Op b.operator Expr();                      \
  }

DEFINE_OVERLOAD_SLICE_UNARY_OP(!);
DEFINE_OVERLOAD_SLICE_UNARY_OP(-);
DEFINE_OVERLOAD_SLICE_BINARY_OP(+);
DEFINE_OVERLOAD_SLICE_BINARY_OP(-);
DEFINE_OVERLOAD_SLICE_BINARY_OP(*);
DEFINE_OVERLOAD_SLICE_BINARY_OP(/);
DEFINE_OVERLOAD_SLICE_BINARY_OP(%);
DEFINE_OVERLOAD_SLICE_BINARY_OP(==);
DEFINE_OVERLOAD_SLICE_BINARY_OP(<=);
DEFINE_OVERLOAD_SLICE_BINARY_OP(>=);
DEFINE_OVERLOAD_SLICE_BINARY_OP(!=);
DEFINE_OVERLOAD_SLICE_BINARY_OP(&&);
DEFINE_OVERLOAD_SLICE_BINARY_OP(||);
DEFINE_OVERLOAD_SLICE_BINARY_OP(>>);
DEFINE_OVERLOAD_SLICE_BINARY_OP(<<);
DEFINE_OVERLOAD_SLICE_BINARY_OP(>);  // NOLINT(*)
DEFINE_OVERLOAD_SLICE_BINARY_OP(<);  // NOLINT(*)

}  // namespace tvm

namespace std {
template <>
struct hash<::tvm::Operation> {
  std::size_t operator()(const ::tvm::Operation& k) const {
    return k.hash();
  }
};
}
#endif  // TVM_TENSOR_H_