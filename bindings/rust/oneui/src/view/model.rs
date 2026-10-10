use super::{callbacks, Value, ViewError, VmContext, VmResult};
use crate::sys;
use std::{
    cell::RefCell,
    collections::HashMap,
    rc::{Rc, Weak},
};

/// Safe, typed functions emitted by viewc. No function accepts a raw pointer.
#[doc(hidden)]
pub struct ModelSpec<V: 'static> {
    pub schema: [u8; 32],
    pub model: u32,
    pub get: fn(&V, u32) -> VmResult<Value>,
    pub set: fn(&mut V, u32, &Value, &VmContext) -> VmResult,
    pub invoke: fn(&mut V, u32, &[Value], &VmContext) -> VmResult,
    pub list_info: fn(&V, u32) -> VmResult<sys::OneUiViewListInfo>,
    pub list_key: fn(&V, u32, usize) -> VmResult<String>,
    pub list_item: fn(&V, u32, usize) -> VmResult<ChildModel>,
    pub connect: fn(&mut V, &VmContext) -> VmResult,
    pub disconnect: fn(&mut V, &VmContext),
}

pub struct ViewSpec<V: 'static> {
    pub(crate) ir: &'static [u8],
    pub(crate) model: ModelSpec<V>,
}
impl<V> ViewSpec<V> {
    /// Typed descriptor construction used by the generated adapter macro.
    #[doc(hidden)]
    pub fn from_generated(ir: &'static [u8], model: ModelSpec<V>) -> Self {
        Self { ir, model }
    }
}

/// Safe erased child descriptor. Only the binding can obtain its FFI pointers.
#[doc(hidden)]
pub struct ChildModel(pub(crate) Rc<Control>);
impl<V: 'static> ModelSpec<V> {
    pub fn bind(self, vm: Rc<RefCell<V>>) -> ChildModel {
        let identity = Rc::as_ptr(&vm) as usize;
        let table = callbacks::table(self.schema, self.model);
        ChildModel(Rc::new(Control {
            table,
            identity,
            model: Box::new(Typed { vm, spec: self }),
            scratch: RefCell::new(Value::Bool(false)),
            connections: RefCell::new(VmContext::default()),
            children: RefCell::new(HashMap::new()),
        }))
    }
}

pub(crate) trait Operations {
    fn get(&self, id: u32) -> VmResult<Value>;
    fn set(&self, id: u32, value: &Value, cx: &VmContext) -> VmResult;
    fn invoke(&self, id: u32, args: &[Value], cx: &VmContext) -> VmResult;
    fn list_info(&self, id: u32) -> VmResult<sys::OneUiViewListInfo>;
    fn list_key(&self, id: u32, index: usize) -> VmResult<String>;
    fn list_item(&self, id: u32, index: usize) -> VmResult<ChildModel>;
    fn connect(&self, cx: &VmContext) -> VmResult;
    fn disconnect(&self, cx: &VmContext) -> VmResult;
}
struct Typed<V: 'static> {
    vm: Rc<RefCell<V>>,
    spec: ModelSpec<V>,
}
impl<V: 'static> Operations for Typed<V> {
    fn get(&self, id: u32) -> VmResult<Value> {
        (self.spec.get)(
            &*self.vm.try_borrow().map_err(|_| ViewError::REENTRANT)?,
            id,
        )
    }
    fn set(&self, id: u32, value: &Value, cx: &VmContext) -> VmResult {
        (self.spec.set)(
            &mut *self.vm.try_borrow_mut().map_err(|_| ViewError::REENTRANT)?,
            id,
            value,
            cx,
        )
    }
    fn invoke(&self, id: u32, args: &[Value], cx: &VmContext) -> VmResult {
        (self.spec.invoke)(
            &mut *self.vm.try_borrow_mut().map_err(|_| ViewError::REENTRANT)?,
            id,
            args,
            cx,
        )
    }
    fn list_info(&self, id: u32) -> VmResult<sys::OneUiViewListInfo> {
        (self.spec.list_info)(
            &*self.vm.try_borrow().map_err(|_| ViewError::REENTRANT)?,
            id,
        )
    }
    fn list_key(&self, id: u32, index: usize) -> VmResult<String> {
        (self.spec.list_key)(
            &*self.vm.try_borrow().map_err(|_| ViewError::REENTRANT)?,
            id,
            index,
        )
    }
    fn list_item(&self, id: u32, index: usize) -> VmResult<ChildModel> {
        (self.spec.list_item)(
            &*self.vm.try_borrow().map_err(|_| ViewError::REENTRANT)?,
            id,
            index,
        )
    }
    fn connect(&self, cx: &VmContext) -> VmResult {
        (self.spec.connect)(
            &mut *self.vm.try_borrow_mut().map_err(|_| ViewError::REENTRANT)?,
            cx,
        )
    }
    fn disconnect(&self, cx: &VmContext) -> VmResult {
        (self.spec.disconnect)(
            &mut *self.vm.try_borrow_mut().map_err(|_| ViewError::REENTRANT)?,
            cx,
        );
        Ok(())
    }
}
pub(crate) struct Control {
    pub table: sys::OneUiViewModelVTable,
    pub identity: usize,
    pub model: Box<dyn Operations>,
    pub scratch: RefCell<Value>,
    pub connections: RefCell<VmContext>,
    pub children: RefCell<HashMap<(u32, usize, u32), Weak<Control>>>,
}
impl Control {
    pub fn borrowed(this: &Rc<Self>) -> sys::OneUiViewModelRef {
        sys::OneUiViewModelRef {
            vtable: &this.table,
            vm: Rc::as_ptr(this).cast_mut().cast(),
        }
    }
    pub fn child(&self, property: u32, index: usize) -> VmResult<Rc<Control>> {
        let child = self.model.list_item(property, index)?.0;
        let key = (property, child.identity, child.table.model_id);
        let mut children = self
            .children
            .try_borrow_mut()
            .map_err(|_| ViewError::REENTRANT)?;
        // Weak cache keeps RESET VM/vtable identity stable without retaining removed rows.
        if let Some(existing) = children.get(&key).and_then(Weak::upgrade) {
            return Ok(existing);
        }
        children.insert(key, Rc::downgrade(&child));
        Ok(child)
    }
}

/// Generated code supplies typed closures; FFI and ownership stay inside OneUI.
#[macro_export]
macro_rules! define_view_adapter {
    ($visibility:vis fn $name:ident<$v:ident: $bound:path>() { ir = $ir:expr; model = $model:expr; }) => {
        $visibility fn $name<$v: $bound>() -> $crate::view::ViewSpec<$v> {
            $crate::view::ViewSpec::from_generated($ir, $model)
        }
    };
}
